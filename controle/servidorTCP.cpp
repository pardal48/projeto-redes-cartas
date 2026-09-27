
#include <iostream>
#include <string>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "servidorTCP.hpp"

servidorTCP:: servidorTCP(int port){

    servidorSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (servidorSocket == -1) {
        perror("erro ao criar socket");
    }
    int opt = 1;
    setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // permite reiniciar a porta rapidamente, pra testes

    sockaddr_in servidor{};
    servidor.sin_family = AF_INET;//ipv4
    servidor.sin_port = htons(port);//port
    servidor.sin_addr.s_addr = INADDR_ANY;//qualquer um conecta

    if (bind(servidorSocket, (sockaddr*)&servidor, sizeof(servidor)) == -1) {
        perror("Erro ao fazer bind");
        close(servidorSocket);
        servidorSocket = -1;
    }


}

servidorTCP::~servidorTCP(){
    if (servidorSocket != -1) {
        close(servidorSocket);
    }
}

void servidorTCP::escutar() {
    if (listen(servidorSocket, SOMAXCONN) == -1) {
        std::cerr << "erro listen" << std::endl;
    }
}

int servidorTCP::aceitar(){
    sockaddr_in cliente{};
    socklen_t tamanho = sizeof(cliente);

    int clientID = accept(servidorSocket, (sockaddr*)&cliente, &tamanho);
    if (clientID == -1) {
        std:: cerr<<"erro aceitar"<<std ::endl;
    }

    return clientID;
}

void servidorTCP::enviar(int clientID, const std::string& mensagem){
    send(clientID, mensagem.data(), mensagem.size(), 0);
}
//em receber podemos por outros tipos pra serem recebidos, isso é só um place holder
std::string servidorTCP::receber(int clienteID){
    char buffer[1024];

    int bytes = recv(clienteID,buffer,sizeof(buffer),0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(buffer,bytes);

}