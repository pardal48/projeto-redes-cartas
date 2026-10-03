# include "Interface.hpp"
#include <iostream>
#include <string>
#include<termios.h>
#include <unistd.h>
#include <poll.h>

/* essas duas funções são pra tentar implementar a visualização e escolha dentre vários servidores disponíveis,
mas eu sou um acéfalo e passei horas tentando e tomei no diff do c++, ignorem por enquanto */

/*
void Interface::mostrar_servidores(const std::vector<ServidorInfo>& servidores) {
    std::cout << "\n  === Servidores disponíveis ===\n";
    for (size_t i = 0; i < servidores.size(); ++i) {
        const ServidorInfo& servidor = servidores[i];
        std::cout << i + 1 << ". " << servidor.nome << ", Jogadores: " << servidor.jogadores
                  << "/" << servidor.capacidade << "\n";
    }
}
void Interface::escolher_servidor(const std::vector<ServidorInfo>& servidores, int& servidorEscolhido){
    int escolha;
    std::cout << "Escolha um servidor :";
    std::cin >> escolha;
    servidorEscolhido = escolha - 1;
    if(servidorEscolhido < 0 || static_cast<size_t>(servidorEscolhido) >= servidores.size()) {
        std::cout << "Escolha inválida. Tente novamente.\n";
        escolher_servidor(servidores, servidorEscolhido);
    }
    else if(servidores[servidorEscolhido].estahCheio()) {
        std::cout << "Servidor cheio. Escolha outro servidor.\n";
        escolher_servidor(servidores, servidorEscolhido);
    }
    else {
        std::cout << "Você escolheu o servidor: " << servidores[servidorEscolhido].nome << "\n";
    }
}
*/

char Interface::lerTecla() {
    termios antigo{};
    tcgetattr(STDIN_FILENO, &antigo);        // salva configuração atual

    termios novo = antigo;
    novo.c_lflag &= ~(ICANON | ECHO);        // desliga buffer de linha e eco
    tcsetattr(STDIN_FILENO, TCSANOW, &novo);

    char c = 0;
    read(STDIN_FILENO, &c, 1);               // lê 1 caractere

    tcsetattr(STDIN_FILENO, TCSANOW, &antigo); // restaura o terminal
    return c;
}

bool Interface::TelaInicial() {
    std::cout << "=== Bem-vindo ao Jogo de Cartas ===\n";
    std::cout << " aperte qualquer tecla para continuar\n";
    std:: cout << "ESC para sair\n";
    
    char tecla = lerTecla();
    if (tecla == 27) { // Código ASCII para ESC
        std::cout << "Saindo do jogo...\n";
        return false;
    }
    return true;
}


// ---------- nome ----------
 
bool Interface::pedir_nome(std::string& nome) {
    {
        std::lock_guard<std::mutex> lock(mtxTela);
        std::cout << "Escolha seu nome (letras e numeros, ate 12): " << std::flush;
    }
    // getline fora do lock: ele bloqueia esperando o usuário, e a thread de
    // recepção não pode ficar impedida de imprimir enquanto isso
    return static_cast<bool>(std::getline(std::cin, nome));
}
 
void Interface::mostrar_nome_invalido() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "Nome invalido ou ja em uso. Tente outro.\n";
}
 
// ---------- lobby ----------
 
void Interface::mostrar_lobby(const ClienteTCP::ListaLobby& jogadoresdoLobby,int meuId, int capacidade) {
    std::lock_guard<std::mutex> lock(mtxTela);
    // Mostra a lista de jogadores do lobby, indicando quem é o próprio jogador e se cada um está pronto ou não
    std::cout << "\n=== LOBBY (" << jogadoresdoLobby.size() << "/" << capacidade << ") ===\n";
    for (const auto& j : jogadoresdoLobby) {
        std::cout << "  " << (*j).getNome() << ((*j).getId() == meuId ? " (voce)" : "") << "  "
                  << ((*j).getPronto() ? "[PRONTO]" : "[aguardando]") << "\n";
    }
    std::cout << "Digite P + Enter para alternar pronto, S + Enter para sair.\n";
    std::cout.flush();// força a saída imediata, sem esperar o buffer encher
}
 
Interface::ComandoLobby Interface::ler_comando_lobby(int timeoutMs) {
    // Terminal normal: o poll só acusa dados quando o usuário já apertou Enter,
    // então o getline abaixo não trava.
    pollfd p{STDIN_FILENO, POLLIN, 0};
    if (poll(&p, 1, timeoutMs) <= 0) return ComandoLobby::Nenhum;
 
    std::string linha;
    if (!std::getline(std::cin, linha)) return ComandoLobby::Sair;  // Ctrl+D
 
    if (linha == "p" || linha == "P") return ComandoLobby::AlternarPronto;
    if (linha == "s" || linha == "S") return ComandoLobby::Sair;
    return ComandoLobby::Nenhum;
}
 
// ---------- mensagens gerais ----------
 
void Interface::mostrar_partida_iniciando() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nA partida vai comecar!\n";
}
 
void Interface::mostrar_conexao_perdida() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nConexao com o servidor perdida.\n";
}
 
void Interface::mostrar_erro(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cerr << mensagem << "\n";
}
