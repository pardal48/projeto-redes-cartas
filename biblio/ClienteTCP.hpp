

#pragma once

#include <iostream>
#include <string>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
class ClienteTCP{
private:
    /* data */
    int clienteID;//id do socket
public:
    ClienteTCP(/* args */);//cria socket
    ~ClienteTCP();
;

void conectar(const std::string& ip, int port);

void enviar(const std::string& mensagem);

std::string receber();

void fechar();

};

