#include <signal.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "ServidorTCP.hpp"

namespace {

ServidorTCP* g_servidor = nullptr;  // acessado pelo signal handler

// Ctrl+C / SIGTERM: só pede para parar; o loop de accept() encerra tudo de forma limpa.
void tratarSinal(int) {
    if (g_servidor) g_servidor->parar();
}

}  // namespace

// Uso: ./servidor [porta]   (padrão 5000)
int main(int argc, char** argv) {
    const int porta = (argc > 1) ? std::atoi(argv[1]) : 5000;

    std::string nomeServidor;
    std::cout << "Digite o nome do servidor: ";
    std::getline(std::cin, nomeServidor);

    ServidorTCP servidor(porta, nomeServidor);
    if (!servidor.ok()) return 1;
    g_servidor = &servidor;

    // sigaction sem SA_RESTART: o poll() é interrompido pelo sinal.
    struct sigaction sa{};
    sa.sa_handler = tratarSinal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    servidor.executar();
    return 0;
}