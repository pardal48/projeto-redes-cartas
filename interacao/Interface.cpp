# include "Interface.hpp"
#include <iostream>
#include <string>
#include<termios.h>
#include <unistd.h>

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