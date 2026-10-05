#include "Interface.hpp"

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <iostream>

namespace {

std::string aparar(const std::string& s) {
    size_t i = 0, f = s.size();
    while (i < f && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    while (f > i && std::isspace(static_cast<unsigned char>(s[f - 1]))) --f;
    return s.substr(i, f - i);
}

}  // namespace

// =====================================================================
// Entrada
// =====================================================================

// Lê uma tecla sem esperar Enter nem mostrar eco.
char Interface::lerTecla() {
    termios antigo{};
    tcgetattr(STDIN_FILENO, &antigo);

    termios novo = antigo;
    novo.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &novo);

    char c = 0;
    ssize_t n = read(STDIN_FILENO, &c, 1);

    tcsetattr(STDIN_FILENO, TCSANOW, &antigo);  // restaura o terminal
    return n > 0 ? c : 27;                      // fim da entrada = sair
}

// Devolve uma linha completa do buffer; se não houver, lê do teclado (com timeout opcional).
Interface::Leitura Interface::lerLinha(std::string& linha, int timeoutMs) {
    while (true) {
        size_t pos = bufferEntrada.find('\n');
        if (pos != std::string::npos) {
            linha = bufferEntrada.substr(0, pos);
            bufferEntrada.erase(0, pos + 1);
            if (!linha.empty() && linha.back() == '\r') linha.pop_back();
            return Leitura::Linha;
        }

        if (timeoutMs >= 0) {
            pollfd p{STDIN_FILENO, POLLIN, 0};
            int r = poll(&p, 1, timeoutMs);
            if (r == 0) return Leitura::Timeout;
            if (r < 0) {
                if (errno == EINTR) continue;
                return Leitura::Fim;
            }
        }

        char tmp[256];
        ssize_t n = read(STDIN_FILENO, tmp, sizeof(tmp));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return Leitura::Fim;
        bufferEntrada.append(tmp, static_cast<size_t>(n));
    }
}

// =====================================================================
// Início e nome
// =====================================================================

bool Interface::TelaInicial() {
    std::cout << "=== Bem-vindo ao Jogo de Cartas ===\n"
              << " aperte qualquer tecla para continuar\n"
              << "ESC para sair\n";

    if (lerTecla() == 27) {  // ESC
        std::cout << "Saindo do jogo...\n";
        return false;
    }
    return true;
}

bool Interface::pedir_nome(std::string& nome) {
    {
        std::lock_guard<std::mutex> lock(mtxTela);
        std::cout << "Escolha seu nome (letras e numeros, ate 12): " << std::flush;
    }
    // Leitura fora do lock: bloqueia esperando o usuário, e a thread de recepção
    // não pode ficar impedida de imprimir enquanto isso.
    if (lerLinha(nome, -1) != Leitura::Linha) return false;
    nome = aparar(nome);
    return true;
}

void Interface::mostrar_nome_invalido() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "Nome invalido ou ja em uso. Tente outro.\n";
}

// =====================================================================
// Lobby
// =====================================================================

void Interface::mostrar_lobby(const ClienteTCP::ListaLobby& jogadores, int meuId, int capacidade) {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\n=== LOBBY (" << jogadores.size() << "/" << capacidade << ") ===\n";
    for (const auto& j : jogadores) {
        std::cout << "  " << j->getNome() << (j->getId() == meuId ? " (voce)" : "") << "  "
                  << (j->getPronto() ? "[PRONTO]" : "[aguardando]") << "\n";
    }
    std::cout << "Digite P + Enter para alternar pronto, S + Enter para sair.\n" << std::flush;
}

Interface::ComandoLobby Interface::ler_comando_lobby(int timeoutMs) {
    std::string linha;
    switch (lerLinha(linha, timeoutMs)) {
        case Leitura::Timeout: return ComandoLobby::Nenhum;
        case Leitura::Fim:     return ComandoLobby::Sair;  // Ctrl+D
        case Leitura::Linha:   break;
    }
    linha = aparar(linha);
    if (linha == "p" || linha == "P") return ComandoLobby::AlternarPronto;
    if (linha == "s" || linha == "S") return ComandoLobby::Sair;
    return ComandoLobby::Nenhum;
}

// =====================================================================
// Partida
// =====================================================================

void Interface::mostrar_partida_iniciando() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nA partida vai comecar!\n";
}

// A mesa chega como segmentos separados por '|'. O último é o histórico do descarte,
// que só aparece com o comando DESCARTE.
std::vector<std::string> Interface::dividirSegmentos(const std::string& estadoMesa) {
    std::vector<std::string> segmentos;
    std::string atual;
    for (char c : estadoMesa) {
        if (c == '|') { segmentos.push_back(aparar(atual)); atual.clear(); }
        else          { atual += c; }
    }
    segmentos.push_back(aparar(atual));
    return segmentos;
}

void Interface::imprimirPrompt(ModoJogo modo) {
    switch (modo) {
        case ModoJogo::MinhaVez:
            std::cout << "Comandos: COMPRAR | JOGAR <num_carta> | COMBO <num_carta> <num_carta> ...> | DESCARTE | MESA | SAIR\n"
                      << "O que deseja fazer? > " << std::flush;
            break;
        case ModoJogo::Reagir:
            std::cout << "Digite JOGAR_NAO ou PASSO > " << std::flush;
            break;
        case ModoJogo::Eliminado:
            std::cout << "Voce foi eliminado e esta assistindo. (SAIR para sair)\n" << std::flush;
            break;
        case ModoJogo::EscolherAlvo:
            std::cout << "Nome do jogador > " << std::flush;
            break;
        case ModoJogo::EscolherCarta:
            std::cout << "Numero da carta a entregar > " << std::flush;
            break;
        case ModoJogo::EscolherTipoCarta:
            std::cout << "Tipo da carta a ser roubada > " << std::flush;
            break;
        case ModoJogo::Aguardar:
            std::cout << "Aguardando a jogada dos outros jogadores...\n" << std::flush;
            break;
    }
}

void Interface::mostrar_mesa(const std::string& estadoMesa, ModoJogo modo) {
    std::lock_guard<std::mutex> lock(mtxTela);
    const auto seg = dividirSegmentos(estadoMesa);
    const size_t visiveis = seg.size() > 1 ? seg.size() - 1 : seg.size();  // esconde o histórico

    std::cout << "\n================= MESA DE JOGO =================\n";
    for (size_t i = 0; i < visiveis; ++i) std::cout << seg[i] << "\n";
    std::cout << "================================================\n";
    imprimirPrompt(modo);
}

void Interface::mostrar_descarte(const std::string& estadoMesa) {
    std::lock_guard<std::mutex> lock(mtxTela);
    const auto seg = dividirSegmentos(estadoMesa);
    std::cout << "\n--- PILHA DE DESCARTE ---\n" << (seg.empty() ? "" : seg.back()) << "\n";
}

void Interface::mostrar_prompt(ModoJogo modo) {
    std::lock_guard<std::mutex> lock(mtxTela);
    imprimirPrompt(modo);
}

void Interface::mostrar_evento(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\n>>> " << mensagem << "\n" << std::flush;
}

std::string Interface::ler_comando_jogo(int timeoutMs) {
    std::string linha;
    switch (lerLinha(linha, timeoutMs)) {
        case Leitura::Timeout: return "";
        case Leitura::Fim:     return "SAIR";
        case Leitura::Linha:   break;
    }
    linha = aparar(linha);
    for (auto& c : linha) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return linha;
}

// =====================================================================
// Mensagens gerais
// =====================================================================

void Interface::mostrar_conexao_perdida() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nConexao com o servidor perdida.\n";
}

void Interface::mostrar_erro(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cerr << mensagem << "\n";
}