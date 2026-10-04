#include "ClienteTCP.hpp"
#include "Interface.hpp"
 
#include <atomic>
#include <cstdlib>
#include <string>
 
// ---- MÚLTIPLOS SERVIDORES (desativado) ----
// Para reativar: troque '#if 0' por '#if 1' (aqui, em ClienteTCP.hpp, clienteTCP.cpp
// e Interface.hpp), reative o comando INFO em servidorTCP.cpp e use o bloco do main.
// Quando reativar, o menu abaixo pode ser movido para a Interface (mostrar_servidores /
// escolher_servidor), no mesmo estilo dos outros métodos.
#if 0
#include "Conexao.hpp"  // ServidorInfo (ajuste o nome do arquivo)
#include <iostream>
#include <thread>
#include <vector>
 
// Uma thread por servidor: o menu não espera 2 s × N servidores.
// Cada thread mexe em um ServidorInfo e em um índice de 'respondeu' só seu
// (vector<char> e não vector<bool>, para não haver corrida entre índices vizinhos).
static void sondarServidores(std::vector<ServidorInfo>& servidores, std::vector<char>& respondeu) {
    respondeu.assign(servidores.size(), 0);
    std::vector<std::thread> sondas;
    for (size_t i = 0; i < servidores.size(); ++i) {
        sondas.emplace_back([&servidores, &respondeu, i] {
            respondeu[i] = consultarServidor(servidores[i]) ? 1 : 0;  // preenche o ServidorInfo
        });
    }
    for (auto& t : sondas) t.join();
}
 
// Devolve o índice escolhido, ou -1 para sair.
static int escolherServidor(std::vector<ServidorInfo>& servidores) {
    while (true) {
        std::vector<char> respondeu;
        sondarServidores(servidores, respondeu);
 
        std::cout << "\n=== Servidores ===\n";
        for (size_t i = 0; i < servidores.size(); ++i) {
            const auto& s = servidores[i];
            std::cout << "  " << (i + 1) << ") " << s.nome << " (" << s.endereco << ":" << s.porta << ")  ";
            if (!respondeu[i])         std::cout << "[offline]\n";
            else if (s.estahCheio())   std::cout << s.jogadores << "/" << s.capacidade << " [cheio]\n";
            else                       std::cout << s.jogadores << "/" << s.capacidade << "\n";
        }
        std::cout << "Numero do servidor, 'r' para atualizar ou '0' para sair: " << std::flush;
 
        std::string entrada;
        if (!std::getline(std::cin, entrada)) return -1;
        if (entrada == "r" || entrada == "R") continue;
        if (entrada == "0") return -1;
 
        int n = std::atoi(entrada.c_str());
        if (n < 1 || static_cast<size_t>(n) > servidores.size()) {
            std::cout << "Opcao invalida.\n";
            continue;
        }
        size_t i = static_cast<size_t>(n - 1);
        if (!respondeu[i])               { std::cout << "Servidor offline.\n"; continue; }
        if (servidores[i].estahCheio())  { std::cout << "Servidor cheio.\n";   continue; }
        return n - 1;
    }
}
#endif

int main() {
   
    
    Jogador jogador;
    Interface interface;
    //se implementar a parte de múltiplos servidores, podemos mudar a parte de escolher servidor para usar a função escolherServidor() que está comentada no main.cpp, mas por enquanto vamos deixar assim mesmo, com o servidor hardcoded para localhost:5000
    //para escolher um servidor da lista de servidores conectados
    std::string ip = "localhost";
    int porta = 5000;
     if (!interface.TelaInicial()) {return 0;}
        // ---- MÚLTIPLOS SERVIDORES (desativado) ----
    // Substitui a conexão direta abaixo: monta a lista, mostra o menu e usa o escolhido.
#if 0
    std::vector<ServidorInfo> servidores;
    servidores.emplace_back("Sala Local", porta);
    servidores.back().endereco = ip;
 
    int escolhido = escolherServidor(servidores);
    if (escolhido < 0) return 0;
    const ServidorInfo& alvo = servidores[static_cast<size_t>(escolhido)];
    ip = alvo.endereco;
    porta = alvo.porta;
#endif
    ClienteTCP cliente;
   
    std::atomic<bool> noLobby{false};
    // Definido antes de conectar(): a thread de recepção lê este callback.
    // Ele roda na thread de recepção; a Interface cuida do mutex da tela.
    //magia negra aqui, depois descubro
    cliente.aoAtualizar = [&] {
        if (noLobby) interface.mostrar_lobby(cliente.lobby(), cliente.meuId());
    };
 
    if (!cliente.conectar(ip, porta)) {
        interface.mostrar_erro("Nao foi possivel conectar ao servidor.");
        return 1;
    }
 
    //escolha do nome
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
 
    // lobby
    noLobby = true;
    interface.mostrar_lobby(cliente.lobby(), cliente.meuId());
 
    while (cliente.conectado() && !cliente.iniciou()) {
        // timeout de 200 ms: reavalia conectado()/iniciou() mesmo sem o usuário digitar
        Interface::ComandoLobby cmd = interface.ler_comando_lobby(200);
        if (cmd == Interface::ComandoLobby::AlternarPronto)
            cliente.enviar(cliente.estouPronto() ? "ESPERA" : "PRONTO");
        else if (cmd == Interface::ComandoLobby::Sair)
            break;
    }
 
    noLobby = false;
    if (cliente.iniciou()) {
        interface.mostrar_partida_iniciando();
        
        // --- LOOP PRINCIPAL DO JOGO ---
        cliente.enviar("MESA");
        cliente.esperarMesa(); // Aguarda a foto inicial da mesa
        
        while (cliente.conectado()) {
            
            std::string estado = cliente.obterEstadoMesaLocal();
            
            // Antes de pedir o comando, verifique de quem é a vez
            while (cliente.obterTurnoAtual() != cliente.meuId() && cliente.conectado()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Espera meio segundo e checa de novo
            }       
            if (!cliente.conectado()) break; // Sai se a conexão cair enquanto esperava

            // Só desenha a mesa e pede a ação se for o turno dele
            interface.mostrar_mesa(estado, false);
            std::string acao = interface.ler_comando_jogo();
            if (acao != "MESA" && acao != "SAIR" && acao != "DESCARTE" && !acao.empty()) {
                cliente.setTurnoAtual(-1); 
            }

            if (acao == "SAIR") {
                break;
            } else if (acao == "DESCARTE") {
                // Mostra a mesa forçando a exibição do descarte (não interage com a rede)
                interface.mostrar_mesa(estado, true);
                continue; 
            } else if (!acao.empty()) {
                cliente.enviar(acao); // Envia o comando (ex: JOGAR 0)
                
                // Em vez de sleep, pede a mesa atualizada e aguarda de forma segura
                cliente.enviar("MESA");
                cliente.esperarMesa(); 
            }
        }
        
    } else if (!cliente.conectado()) {
        interface.mostrar_conexao_perdida();
    }

}