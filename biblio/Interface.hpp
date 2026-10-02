#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "conexao.hpp"

class Interface {
public:
    std:: vector<ServidorInfo> servidores; // Lista de servidores disponíveis
    
    void mostrar_servidores(const std::vector<ServidorInfo>& servidores);//vetor de servidores conectados, para mostrar na tela inicial

    void escolher_servidor(const std::vector<ServidorInfo>& servidores, int& servidorEscolhido);//escolher um servidor da lista de servidores conectados
    
    char lerTecla(); // Função para ler uma tecla do teclado sem precisar pressionar Enter
    bool TelaInicial();

    void menuPrincipal();

    void Lobby();
    void Game();



};