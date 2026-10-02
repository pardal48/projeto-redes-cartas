#pragma once
#include <iostream>
#include <string>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

class servidorTCP {

private:

    int servidorSocket;
    int porta;
public:

    servidorTCP(int port);

    ~servidorTCP();

    void escutar();

    int aceitar();

    void enviar(int clientID,const std::string& mensagem);

    std::string receber(int clientID);
    
    int getporta() const;
       
};

