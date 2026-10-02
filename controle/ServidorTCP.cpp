
#include <iostream>
#include <string>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ServidorTCP.hpp"

ServidorTCP::ServidorTCP(int port, const std::string& nome) : porta(port), nomeServidor(nome) {
    servidorSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (servidorSocket == -1) {
        perror("erro ao criar socket");
    }
    int opt = 1;
    setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // permite reiniciar a porta rapidamente, pra testes

    sockaddr_in servidor{};
    servidor.sin_family = AF_INET;//ipv4
    servidor.sin_port = htons(port);//porta
    servidor.sin_addr.s_addr = INADDR_ANY;//qualquer um conecta

    if (bind(servidorSocket, (sockaddr*)&servidor, sizeof(servidor)) == -1) {
        perror("Erro ao fazer bind");
        close(servidorSocket);
        servidorSocket = -1;
    }

    sockaddr_in enderecoReal{};
    socklen_t tamanho = sizeof(enderecoReal);
    if (getsockname(servidorSocket, (sockaddr*)&enderecoReal, &tamanho) == -1) {
        perror("Erro ao obter a porta real");
        close(servidorSocket);
        servidorSocket = -1;
    } else {
        porta = ntohs(enderecoReal.sin_port);//pega a porta gerada pelo SO, caso a porta passada seja 0

    }
}

ServidorTCP::~ServidorTCP(){
    if (servidorSocket != -1) {
        close(servidorSocket);
    }
}

void ServidorTCP::escutar() {
    if (listen(servidorSocket, SOMAXCONN) == -1) {
        std::cerr << "erro listen" << std::endl;
    }
}

int ServidorTCP::aceitar(){
    sockaddr_in cliente{};
    socklen_t tamanho = sizeof(cliente);

    int clientID = accept(servidorSocket, (sockaddr*)&cliente, &tamanho);
    if (clientID == -1) {
        std:: cerr<<"erro aceitar"<<std ::endl;
    }

    return clientID;
}

void ServidorTCP::enviar(int clientID, const std::string& mensagem){
    send(clientID, mensagem.data(), mensagem.size(), 0);
}
//em receber podemos por outros tipos pra serem recebidos, isso é só um place holder
std::string ServidorTCP::receber(int clienteID){
    char buffer[1024];

    int bytes = recv(clienteID,buffer,sizeof(buffer),0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(buffer,bytes);

}

int ServidorTCP::getporta() const {
    return porta;
}

