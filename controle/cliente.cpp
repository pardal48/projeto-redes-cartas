#include <iostream>
#include <string>
#include <vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include"clienteTCP.hpp"
#include"Interface.hpp"
#include"conexao.hpp"
int main() {
   
    clienteTCP cliente;
    Jogador jogador;
    Interface interface;
    
    if (!interface.TelaInicial()) {
        return 0;
    }
    std::cout << "[CLIENTE] A tentar conectar ao servidor...\n";
    cliente.conectar("127.0.0.1", 5000);
        
    //comece a implementar a partir daqui, pode começar pelo lobby ou podemos pular essa parte e ir direto pro jogo, 
    //mas o lobby é importante pra ver se o servidor está cheio ou não, e também pra ver se tem o mínimo de 2 jogadores


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