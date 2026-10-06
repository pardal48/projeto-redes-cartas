#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

#include "ClienteTCP.hpp"
#include "Interface.hpp"

// Uso: ./cliente [host] [porta]   (padrão localhost 5000)
int main(int argc, char** argv) {
    const std::string ip = (argc > 1) ? argv[1] : "localhost";
    const int porta = (argc > 2) ? std::atoi(argv[2]) : 5000;

    Interface interface;
    if (!interface.TelaInicial()) return 0;

    ClienteTCP cliente;
    std::atomic<bool> noLobby{false};
    std::atomic<bool> emPartida{false};

    // desenho com anti-duplicação
    // sem o mutex era frequente a impressão da mesa múltiplas vezes, o que não atrapalhava o funcionamento
    //mas prejudicava a vizualização do usuário
    // A thread de recepção e a thread principal podem pedir o mesmo desenho ao mesmo tempo.
    // O mutex + o número de versão garantem que cada versão do estado é impressa uma vez.
    std::mutex mtxDesenho;
    unsigned ultimaVersaoLobby = 0, ultimaVersaoMesa = 0;

    auto desenharLobby = [&] {//desenha o lobby pro usuário
        std::lock_guard<std::mutex> lock(mtxDesenho);
        auto foto = cliente.snapshotLobby();
        if (foto.versao == ultimaVersaoLobby) return;
        ultimaVersaoLobby = foto.versao;
        interface.mostrar_lobby(foto.jogadores, cliente.meuId());
    };

    auto desenharMesa = [&] {//desenha a mesa
        std::lock_guard<std::mutex> lock(mtxDesenho);
        auto foto = cliente.snapshotMesa();
        if (foto.versao == ultimaVersaoMesa || foto.texto.empty()) return;
        ultimaVersaoMesa = foto.versao;
        interface.mostrar_mesa(foto.texto, cliente.modoAtual());
    };

    // Callbacks definidos ANTES de conectar: a thread de recepção os lê.
    cliente.aoAtualizar = [&] {
        if (noLobby)        desenharLobby();
        else if (emPartida) desenharMesa();
    };

    // mostra/pede pra mostra um evento no jogo, que pode ser dois casos, ou erro ou a pergunta pra jogar o não
    cliente.aoEvento = [&](const std::string& texto, bool ehErro) {
        interface.mostrar_evento(texto);
        if (!emPartida) return;
        // Após um erro ou uma pergunta de reação mostra de novo o que o jogador pode digitar.
        const ModoJogo modo = cliente.modoAtual();
        if (ehErro || modo == ModoJogo::Reagir || modo == ModoJogo::EscolherAlvo ||
            modo == ModoJogo::EscolherCarta)
            interface.mostrar_prompt(modo);
    };

    // conexão com o servidor
    std::cout << "Conectando ao servidor (" << ip << ":" << porta << ")...\n";
    if (!cliente.conectar(ip, porta)) {
        std::cerr << "Erro: Nao foi possivel conectar ao servidor!\n";
        return 1;
    }

    //  digita o nome do usuário no jogo
    std::string nome;
    bool nomeOk = false;
    while (cliente.conectado()) {
        if (!interface.pedir_nome(nome)) return 0;  // fim da entrada

        cliente.enviar("NOME " + nome);
        if (cliente.esperarNome()) { nomeOk = true; break; }
        if (cliente.conectado()) interface.mostrar_nome_invalido();
    }
    if (!nomeOk) {
        interface.mostrar_erro("Conexao encerrada (lobby cheio, partida em andamento ou servidor fora).");
        return 1;
    }

    //lobby
    noLobby = true;
    desenharLobby();

    while (cliente.conectado() && !cliente.iniciou()) {
        // timeout de 200 ms: reavalia conectado()/iniciou() mesmo sem o usuário digitar
        //avalia a condição de todos prontos por exemplo, ou se um usuário cancelou logo após o último ficar pronto
        switch (interface.ler_comando_lobby(200)) {
            case Interface::ComandoLobby::AlternarPronto://seta se o usuário está pronto ou não
                cliente.enviar(cliente.estouPronto() ? "ESPERA" : "PRONTO");
                break;
            case Interface::ComandoLobby::Sair://sai do lobby
                return 0;
            case Interface::ComandoLobby::Nenhum://mantem como está
                break;
        }
    }
    noLobby = false;

    if (!cliente.iniciou()) {//se o cliente falhar em iniciar a partida
        interface.mostrar_conexao_perdida();
        return 1;
    }

    // partida 
    interface.mostrar_partida_iniciando();
    emPartida = true;
    desenharMesa();  // a mesa pode já ter chegado antes de emPartida ligar

    while (cliente.conectado() && !cliente.terminou()) {//enquanto o jogo acontece
        const std::string comando = interface.ler_comando_jogo(200);
        if (comando.empty()) continue;
        if (comando == "SAIR") break;

        // O modo é lido DEPOIS da digitação: reflete o estado mais recente.
        const ModoJogo modo = cliente.modoAtual();

        if (comando == "MESA") {  // pede a mesa de novo (a resposta chega como MESA_ESTADO)
            cliente.enviar("MESA");
            continue;
        }
        if (comando == "DESCARTE") {//mostra a pilha de descarte para o jogador
            interface.mostrar_descarte(cliente.snapshotMesa().texto);
            interface.mostrar_prompt(modo);//mostra pro jogador se é hora dele fazer alguma coisa ou se ele ainda
            //está aguardando outros jogarem, isso previne que ele se perca quanto ao turno e reações enquanto olha o descarte
            continue;
        }

        switch (modo) {// analisa se o jogador está em modo de reação ou se é o turno dele
            case ModoJogo::Reagir:
                if (comando == "JOGAR_NAO")  cliente.responderNao(true);
                else if (comando == "PASSO") cliente.responderNao(false);
                else interface.mostrar_erro("Digite JOGAR_NAO ou PASSO.");
                break;

            case ModoJogo::MinhaVez:
                // O servidor valida tudo; aqui só se filtra o que nem faz sentido enviar.
                if (comando == "COMPRAR" || comando.rfind("JOGAR", 0) == 0 || comando.rfind("COMBO", 0) == 0) cliente.enviar(comando);
                else {
                    interface.mostrar_erro("Comando invalido.");
                    interface.mostrar_prompt(modo);
                }
                break;

            case ModoJogo::EscolherAlvo://escolhe alvo de uma carta
            case ModoJogo::EscolherCarta://escolhe a carta para ser entregue, como em favor, ou pra roubar a carta do adversário
            case ModoJogo::EscolherTipoCarta://qual tipo de carta roubar, caso dos maiores combos que permitem escolher 
                //precisamente a carta que quer
                cliente.enviar(comando);
                break;

            case ModoJogo::Aguardar://jogador está esperando outro jogador jogar
                interface.mostrar_erro("Ainda nao e a sua vez.");
                break;

            case ModoJogo::Eliminado://jogador está eliminado
                interface.mostrar_erro("Voce foi eliminado. Digite SAIR para sair.");
                break;
        }
    }

    emPartida = false;//saiu do loop: acabou a partida
    if (!cliente.terminou() && !cliente.conectado()) interface.mostrar_conexao_perdida();//se for por perda de conexão, avisa
    return 0;
}