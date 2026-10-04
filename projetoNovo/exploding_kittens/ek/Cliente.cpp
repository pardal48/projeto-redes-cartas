#include "ClienteTCP.hpp"
#include "Interface.hpp"
 
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
 
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

// ---------- comandos da partida: texto digitado -> linha do protocolo ----------
namespace {

enum class Traducao { Enviar, Local, Sair };

std::string emMaiusculas(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string emMinusculas(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool soDigitos(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

// Converte o que o jogador digitou na linha do protocolo (ex.: "j 5,9 ana" -> "JOGAR 5,9 3").
// O servidor revalida tudo: aqui só se traduz nome -> id e se rejeita o que nem faz sentido enviar.
Traducao traduzirComando(const std::string& entrada, const ClienteTCP& cliente, Interface& interface,
                         std::string& saida) {
    std::istringstream is(entrada);
    std::vector<std::string> t;
    std::string w;
    while (is >> w) t.push_back(w);
    if (t.empty()) return Traducao::Local;

    const std::string cmd = emMinusculas(t[0]);
    if (cmd == "ajuda" || cmd == "?" || cmd == "h") { interface.mostrar_ajuda_jogo(); return Traducao::Local; }
    if (cmd == "mao" || cmd == "m")                 { saida = "MAO"; return Traducao::Enviar; }
    if (cmd == "mesa")                              { saida = "MESA"; return Traducao::Enviar; }
    if (cmd == "comprar" || cmd == "c")             { saida = "COMPRAR"; return Traducao::Enviar; }
    if (cmd == "nao" || cmd == "n")                 { saida = "JOGAR_NAO"; return Traducao::Enviar; }
    if (cmd == "sair" || cmd == "s")                { saida = "SAIR"; return Traducao::Sair; }
    if (cmd == "dar" || cmd == "d") {
        if (t.size() != 2) { interface.mostrar_erro("Uso: dar <id_da_carta>"); return Traducao::Local; }
        saida = "DAR " + t[1];
        return Traducao::Enviar;
    }
    if (cmd == "pos" || cmd == "posicao") {
        if (t.size() != 2) { interface.mostrar_erro("Uso: pos <n>"); return Traducao::Local; }
        saida = "POSICAO " + t[1];
        return Traducao::Enviar;
    }
    if (cmd == "jogar" || cmd == "j") {
        if (t.size() < 2 || t.size() > 4) {
            interface.mostrar_erro("Uso: jogar <ids> [alvo] [carta]  (digite 'ajuda')");
            return Traducao::Local;
        }
        saida = "JOGAR " + t[1];
        if (t.size() >= 3) {
            // número = id do jogador; senão tenta como nome
            std::string alvo = t[2];
            if (!soDigitos(alvo)) {
                int id = cliente.idPorNome(alvo);
                if (id < 0) { interface.mostrar_erro("Jogador desconhecido: " + alvo); return Traducao::Local; }
                alvo = std::to_string(id);
            }
            saida += " " + alvo;
        }
        if (t.size() == 4) saida += " " + emMaiusculas(t[3]);
        return Traducao::Enviar;
    }
    interface.mostrar_erro("Comando desconhecido. Digite 'ajuda'.");
    return Traducao::Local;
}

}  // namespace

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
 
    // Mensagens que não são do lobby (contagem, jogo, erros). Também roda na thread de recepção.
    cliente.aoEvento = [&](const std::string& linha) {
        interface.mostrar_evento(linha, cliente);
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
 
    // Laço principal: lobby -> partida -> (fim da partida, o servidor devolve todos ao lobby) -> lobby ...
    bool sair = false;
    while (!sair && cliente.conectado()) {
        // lobby
        noLobby = true;
        interface.mostrar_lobby(cliente.lobby(), cliente.meuId());

        while (cliente.conectado() && !cliente.iniciou()) {
            // timeout de 200 ms: reavalia conectado()/iniciou() mesmo sem o usuário digitar
            Interface::ComandoLobby cmd = interface.ler_comando_lobby(200);
            if (cmd == Interface::ComandoLobby::AlternarPronto)
                cliente.enviar(cliente.estouPronto() ? "ESPERA" : "PRONTO");
            else if (cmd == Interface::ComandoLobby::Sair) {
                sair = true;
                break;
            }
        }

        noLobby = false;
        if (sair || !cliente.iniciou()) break;

        // ---- PARTIDA ----
        // As mensagens do servidor (ORDEM, MAO, TURNO, ...) já são impressas pela thread de recepção
        // via aoEvento. Aqui só lemos o que o jogador digita (com timeout, para perceber FIM / queda).
        interface.mostrar_ajuda_jogo();
        while (cliente.conectado() && !cliente.partidaTerminou()) {
            std::string entrada;
            Interface::Leitura r = interface.ler_linha(200, entrada);
            if (r == Interface::Leitura::Fim) { sair = true; break; }  // Ctrl+D
            if (r == Interface::Leitura::Nada) continue;

            std::string linhaProtocolo;
            Traducao tr = traduzirComando(entrada, cliente, interface, linhaProtocolo);
            if (tr == Traducao::Local) continue;
            if (!cliente.enviar(linhaProtocolo)) break;
            if (tr == Traducao::Sair) { sair = true; break; }
        }
        if (sair || !cliente.partidaTerminou()) break;  // saiu ou a conexão caiu

        cliente.resetarPartida();  // partida terminou: volta ao lobby
    }

    if (!sair && !cliente.conectado()) interface.mostrar_conexao_perdida();

    cliente.fechar();
    return 0;
}
