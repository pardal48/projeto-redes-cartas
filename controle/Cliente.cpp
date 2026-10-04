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

    // ---- desenho com anti-duplicação ----
    // A thread de recepção e a thread principal podem pedir o mesmo desenho ao mesmo tempo.
    // O mutex + o número de versão garantem que cada versão do estado é impressa UMA vez.
    std::mutex mtxDesenho;
    unsigned ultimaVersaoLobby = 0, ultimaVersaoMesa = 0;

    auto desenharLobby = [&] {
        std::lock_guard<std::mutex> lock(mtxDesenho);
        auto foto = cliente.snapshotLobby();
        if (foto.versao == ultimaVersaoLobby) return;
        ultimaVersaoLobby = foto.versao;
        interface.mostrar_lobby(foto.jogadores, cliente.meuId());
    };

    auto desenharMesa = [&] {
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

    cliente.aoEvento = [&](const std::string& texto, bool ehErro) {
        interface.mostrar_evento(texto);
        if (!emPartida) return;
        // Após um erro (ou uma pergunta de reação) mostra de novo o que o jogador pode digitar.
        const ModoJogo modo = cliente.modoAtual();
        if (ehErro || modo == ModoJogo::Reagir) interface.mostrar_prompt(modo);
    };

    // ---- conexão ----
    std::cout << "Conectando ao servidor (" << ip << ":" << porta << ")...\n";
    if (!cliente.conectar(ip, porta)) {
        std::cerr << "Erro: Nao foi possivel conectar ao servidor!\n";
        return 1;
    }

    // ---- nome ----
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

    // ---- lobby ----
    noLobby = true;
    desenharLobby();

    while (cliente.conectado() && !cliente.iniciou()) {
        // timeout de 200 ms: reavalia conectado()/iniciou() mesmo sem o usuário digitar
        switch (interface.ler_comando_lobby(200)) {
            case Interface::ComandoLobby::AlternarPronto:
                cliente.enviar(cliente.estouPronto() ? "ESPERA" : "PRONTO");
                break;
            case Interface::ComandoLobby::Sair:
                return 0;
            case Interface::ComandoLobby::Nenhum:
                break;
        }
    }
    noLobby = false;

    if (!cliente.iniciou()) {
        interface.mostrar_conexao_perdida();
        return 1;
    }

    // ---- partida ----
    interface.mostrar_partida_iniciando();
    emPartida = true;
    desenharMesa();  // a mesa pode já ter chegado antes de emPartida ligar

    while (cliente.conectado() && !cliente.terminou()) {
        const std::string comando = interface.ler_comando_jogo(200);
        if (comando.empty()) continue;
        if (comando == "SAIR") break;

        // O modo é lido DEPOIS da digitação: reflete o estado mais recente.
        const ModoJogo modo = cliente.modoAtual();

        if (comando == "MESA") {  // pede a mesa de novo (a resposta chega como MESA_ESTADO)
            cliente.enviar("MESA");
            continue;
        }
        if (comando == "DESCARTE") {
            interface.mostrar_descarte(cliente.snapshotMesa().texto);
            interface.mostrar_prompt(modo);
            continue;
        }

        switch (modo) {
            case ModoJogo::Reagir:
                if (comando == "JOGAR_NAO")  cliente.responderNao(true);
                else if (comando == "PASSO") cliente.responderNao(false);
                else interface.mostrar_erro("Digite JOGAR_NAO ou PASSO.");
                break;

            case ModoJogo::MinhaVez:
                // O servidor valida tudo; aqui só se filtra o que nem faz sentido enviar.
                if (comando == "COMPRAR" || comando.rfind("JOGAR", 0) == 0) cliente.enviar(comando);
                else {
                    interface.mostrar_erro("Comando invalido.");
                    interface.mostrar_prompt(modo);
                }
                break;

            case ModoJogo::Aguardar:
                interface.mostrar_erro("Ainda nao e a sua vez.");
                break;

            case ModoJogo::Eliminado:
                interface.mostrar_erro("Voce foi eliminado. Digite SAIR para sair.");
                break;
        }
    }

    emPartida = false;
    if (!cliente.terminou() && !cliente.conectado()) interface.mostrar_conexao_perdida();
    return 0;
}