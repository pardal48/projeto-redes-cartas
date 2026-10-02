#include"ClienteTCP.hpp"
#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

ClienteTCP::ClienteTCP(){
    clienteID = socket(AF_INET,SOCK_STREAM,0);
    if (clienteID == -1) {
        perror("Erro ao criar socket do cliente");
    }

}

ClienteTCP::~ClienteTCP(){

        close(clienteID);
}
 

void ClienteTCP::conectar(const std::string& ip,int port){

    sockaddr_in servidor{};

        servidor.sin_family = AF_INET;
        servidor.sin_port = htons(port);

        inet_pton(AF_INET,ip.c_str(),&servidor.sin_addr);

        if (connect(clienteID,(sockaddr*)&servidor,sizeof(servidor)) == -1) {
            perror("Erro ao converter endereço IP");
    }

}

void ClienteTCP::enviar(const std::string & mensagem){

    send(clienteID,mensagem.data(),mensagem.size(),0);
    


}

std:: string ClienteTCP :: receber(){
     char buffer[1024];

    int bytes = recv(clienteID,buffer,sizeof(buffer),0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(
        buffer,
        bytes
    );

}

void ClienteTCP::fechar() {

    if (clienteID!= -1) {

        close(clienteID);

        clienteID = -1;
    }
}

 