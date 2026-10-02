#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ServidorTCP.hpp"
#include "Conexao.hpp"

int main() {


    std:: string nomeServidor;
    std::cout << "Digite o nome do servidor: ";
    std::cin >> nomeServidor;
    
    
    ServidorTCP servidor(5000, nomeServidor); // Porta 5000, se zero, o SO escolhe uma porta disponível

    //ServidorInfo infoServidor(nomeServidor,servidor.getporta());//armazena as informações do servidor, como nome e porta
    std::cout << "[SERVIDOR] A iniciar na porta " << servidor.getporta() << "...\n";
    servidor.escutar();
        
    std::cout << "[SERVIDOR] A aguardar conexao de cliente...\n";
    int clienteID = servidor.aceitar();

    if (clienteID== -1) {
        return 1;
    }

    std::cout << "[SERVIDOR] Cliente conectado com sucesso!\n";

    // 1. Recebe mensagem do cliente
    std::string mensagem_recebida = servidor.receber(clienteID);
    std::cout << "[SERVIDOR] Mensagem do cliente: " << mensagem_recebida << "\n";

    // 2. Envia resposta de volta
    std::string resposta = "Ola Cliente! Mensagem recebida com sucesso.";
    servidor.enviar(clienteID, resposta);
    std::cout << "[SERVIDOR] Resposta enviada.\n";

    // Termina a conexao com o cliente
    close(clienteID);
    return 0;
}