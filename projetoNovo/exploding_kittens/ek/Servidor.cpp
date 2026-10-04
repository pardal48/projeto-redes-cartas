#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include "ServidorTCP.hpp"
#include "Conexao.hpp"
static ServidorTCP* g_servidor = nullptr;// ponteiro global para o servidor, usado no signal handler
static void tratarSinal(int) {// função de tratamento de sinal para Ctrl+C
    //quando o sinal é recebido, chama o método parar() do servidor para encerrar a execução do loop de accept()
    //isso evita que o programa seja encerrado abruptamente e permite que o servidor feche corretamente os sockets e libere recursos
    if (g_servidor) g_servidor->parar();//se o ponteiro global do servidor não for nulo, isto é, se o servidor estiver em execução, 
    //chama o método parar() para encerrar a execução do loop de accept()
}

int main() {


    std:: string nomeServidor;
    std::cout << "Digite o nome do servidor: ";
    std::cin >> nomeServidor;
    
    
    ServidorTCP servidor(5000, nomeServidor); // Porta 5000, se zero, o SO escolhe uma porta disponível

    g_servidor = &servidor;
 
    // sigaction sem SA_RESTART: o poll() é interrompido pelo Ctrl+C
    // basicamente impede que o fechamento abrupto do terminal cause problemas no servidor, 
    //permitindo que ele seja encerrado de forma controlada
    // ia sugeriu usar isso então deixarei para evitar problemas, principalmente durante o desenvolvimento e testes do servidor
    struct sigaction sa{};
    sa.sa_handler = tratarSinal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    

    servidor.executar();
    return 0;
}

    
