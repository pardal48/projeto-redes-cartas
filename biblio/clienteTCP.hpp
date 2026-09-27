

#ifndef CLIENTETCP_HPP
#define CLIENTETCP_HPP

#include <iostream>
#include <string>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
class clienteTCP{
private:
    /* data */
    int clienteID;//id do socket
public:
    clienteTCP(/* args */);//cria socket
    ~clienteTCP();
;

void conectar(const std::string& ip, int port);

void enviar(const std::string& mensagem);

std::string receber();

void fechar();

};

#endif