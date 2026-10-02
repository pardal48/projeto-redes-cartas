#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include "Conexao.hpp"

class ServidorTCP {

public:
    static constexpr size_t MIN_JOGADORES = 2;
    static constexpr size_t MAX_JOGADORES = 5;

     explicit ServidorTCP(int porta, const std::string& nome = "Servidor");
     ~ServidorTCP();
     void escutar();

    int aceitar();

    void enviar(int clientID,const std::string& mensagem);

    std::string receber(int clientID);
    
    int getporta() const;
private:

    int servidorSocket;
    int porta;
    std::string nomeServidor;
    //ServidorTCP(int port);

    

       
};

