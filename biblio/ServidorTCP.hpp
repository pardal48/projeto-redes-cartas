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
// Servidor com uma thread por cliente.
// REGRA: todo acesso a clientes / jogoIniciado / (futuro) estado do jogo
// acontece com 'mtx' travado. Métodos marcados [mtx] exigem o lock já feito.

public:
    //static constexpr size_t MIN_JOGADORES = 2;
    //static constexpr size_t MAX_JOGADORES = 5;

     explicit ServidorTCP(int porta, const std::string& nome = "Servidor");
     ~ServidorTCP();
     void escutar();

    int aceitar();

    void enviar(int clientID,const std::string& mensagem);

    std::string receber(int clientID);
    
    int getporta() const;

    bool iniciar();   // socket, bind, listen (com verificação de erro)
    void executar();  // loop de accept; retorna quando parar() for chamado
    void parar();     // seguro para chamar em signal handler

private:

    void atenderCliente(std::shared_ptr<ClienteConectado> c);  // corpo de cada thread
    std::shared_ptr<ClienteConectado> registrar(int fd);       // trava mtx internamente
    void removerCliente(const std::shared_ptr<ClienteConectado>& c);
    void encerrar();
 
    // [mtx]
    void processarLinha(ClienteConectado& c, const std::string& linha);
    void processarComandoJogo(ClienteConectado& c, const std::string& linha);
    void broadcast(const std::string& msg);
    bool nomeValido(const std::string& nome) const;
    bool todosProntos() const;
    size_t jogadoresNoLobby() const;  // só quem já escolheu nome
    std::string estadoComoTexto() const;
 
    //variaveis
    int porta;
    int servidorSocket = -1;// socket do servidor
    std::string nomeServidor;
    ServidorInfo info;  // identidade do servidor: nome, capacidade, estahCheio() [mtx]
    int listenFd = -1;// socket de escuta do servidor
    std::atomic<bool> rodando{false};
 
    std::mutex mtx;
    std::condition_variable cv;  // usar na janela de reação (wait_for + notify_all)
    std::vector<std::shared_ptr<ClienteConectado>> clientes;//clientes conectados, cada cliente tem um socket e um jogador associado
    bool jogoIniciado = false;//indica se o jogo já começou, para não permitir que novos jogadores entrem no lobby
    int proximoId = 1;
 
    std::vector<std::thread> threads;  // só a thread de accept mexe aqui


    

       
};

