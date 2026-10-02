#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "servidorTCP.hpp"
#include "conexao.hpp"

int main() {
    /*char buffer[1024];
    int servidorSocket = socket(AF_INET, SOCK_STREAM, 0);//IPv4, TCP
    if (servidorSocket == -1) {
        std::cerr << "Erro ao criar o socket do servidor" << std::endl;
        return 1;
    }
    std::cout << "Socket do servidor criado" << std::endl;
    
    sockaddr_in endereco; // Endereço IP do servidor
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(8080); // Porta do servidor
    endereco.sin_addr.s_addr = INADDR_ANY; // Aceitar conexões de qualquer
    
    if(bind(servidorSocket,(sockaddr*)&endereco, sizeof(endereco)) == -1) {
        std::cerr << "Erro no bind cliente" << std::endl;
        return 1;
    }
    std::cout << "socket na porta" << std::endl;
    if(listen(servidorSocket,10)==1){
        std::cerr << "erro listen" << std::endl;
    }
    //aceita um cliente
    int cliente = accept(servidorSocket,nullptr,nullptr);
    std:: cout << "cliente conectado" <<std:: endl;

    int bytes = recv(cliente,buffer,sizeof(buffer),0);
    buffer[bytes]='\0';

    std::cout << "Mensagem: " << buffer << std::endl;
    close(cliente);
    close(servidorSocket);


    return 0;*/

    std:: string nomeServidor;
    std::cout << "Digite o nome do servidor: ";
    std::cin >> nomeServidor;
    
    
    servidorTCP servidor(0);

    ServidorInfo infoServidor(nomeServidor,servidor.getporta());//armazena as informações do servidor, como nome e porta
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