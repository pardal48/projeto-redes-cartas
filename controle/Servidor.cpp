#include <signal.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "ServidorTCP.hpp"

namespace {//evita outros arquivos de verem isso, evita conflito de nomes iguais, tem uma outra função de tratar sinal em outro arquivo

ServidorTCP* g_servidor = nullptr;

void tratarSinal(int) {
    if (g_servidor) g_servidor->parar();
}

}

int main(int argc, char** argv) {
    const int porta = (argc > 1) ? std::atoi(argv[1]) : 5000; // Define a porta padrão em 5000 (ou recebe input do usuário)

    std::string nomeServidor;
    std::cout << "Digite o nome do servidor: ";
    std::getline(std::cin, nomeServidor);

    ServidorTCP servidor(porta, nomeServidor);
    if (!servidor.ok()) return 1;
    g_servidor = &servidor;
    /*
     Handlers de sinal: Ctrl+C (SIGINT) e kill (SIGTERM) só pedem para o servidor parar; o encerramento real acontece 
     no fluxo normal. sa{} zera os flags, ou seja, sem SA_RESTART: o poll() é interrompido pelo sinal (EINTR) e 
     o loop de executar() consegue checar 'rodando'.*/
    struct sigaction sa{};
    sa.sa_handler = tratarSinal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);//código para ctrl +c
    sigaction(SIGTERM, &sa, nullptr); // Código para reagir a comandos do terminal(kill padrão)

    servidor.executar(); // Inicia o servidor para escutar por clientes no socket de escuta na porta definida
    return 0;
}