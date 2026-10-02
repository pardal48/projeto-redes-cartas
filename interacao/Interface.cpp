# include "Interface.hpp"
#include <iostream>
#include <string>


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
    if(servidorEscolhido < 0 || servidorEscolhido >= servidores.size()) {
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

bool Interface::TelaInicial() {
    std::cout << "=== Bem-vindo ao Jogo de Cartas ===\n";
    std::cout << " aperte qualquer tecla para continuar\n";
    std:: cout << "ESC para sair\n";
    
    std:: string opcao;
    std::cin >> opcao;
    if(opcao == "ESC") {
        return false;
    }
    return true;
}