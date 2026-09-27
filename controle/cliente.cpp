#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include"clienteTCP.hpp"
int main() {
   /* std::string mensagem ="ola server";
    int clienteSocket = socket(AF_INET, SOCK_STREAM, 0);//IPv4, TCP
    if (clienteSocket == -1) {
        std::cerr << "Erro ao criar o socket do cliente" << std::endl;
        return 1;
    }
    std::cout << "Socket do cliente criado " << std::endl;

    sockaddr_in enderecoServidor; // Endereço IP do servidor
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(8080); // Porta do servidor
    inet_pton(AF_INET,"127.0.0.1",&enderecoServidor.sin_addr);

    if(connect(clienteSocket,(sockaddr*)&enderecoServidor,sizeof(enderecoServidor))==-1){
        std::cerr << "Erro ao criar o socket do cliente" << std::endl;
        return 1;
    }

    std::cout << "Conectado ao servidor!"<<std::endl;

    send(clienteSocket,mensagem.c_str(),mensagem.size(),0);

    close(clienteSocket);
    

    
    
    return 0;
*/
    clienteTCP cliente;

    std::cout << "[CLIENTE] A tentar conectar ao servidor...\n";
    cliente.conectar("127.0.0.1", 8080);
        
    

    std::cout << "[CLIENTE] Conectado!\n";

    // 1. Envia mensagem inicial
    std::string mensagem = "Ola Servidor! Esta e uma mensagem de teste.";
    cliente.enviar(mensagem);
    std::cout << "[CLIENTE] Mensagem enviada: " << mensagem << "\n";

    // 2. Aguarda a resposta do servidor
    std::string resposta = cliente.receber();
    std::cout << "[CLIENTE] Resposta do servidor: " << resposta << "\n";

    cliente.fechar();
    return 0;

}