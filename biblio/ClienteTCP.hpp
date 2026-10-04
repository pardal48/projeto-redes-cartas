

#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "Jogador.hpp"
// ---- MÚLTIPLOS SERVIDORES (desativado) ----
// Para reativar: troque '#if 0' por '#if 1' (aqui, em clienteTCP.cpp e em cliente.cpp).
#if 0
#include "Conexao.hpp"  // ServidorInfo (ajuste o nome do arquivo)
#endif
 
// Resolve o nome (aceita "localhost" ou IP) e conecta. Devolve o fd, ou -1.
// verboso = false não imprime erros (útil na sondagem de servidores offline).
int conectarTCP(const std::string& host, int porta, bool verboso = true);
 
// ---- MÚLTIPLOS SERVIDORES (desativado) ----
#if 0
// Conecta em info.endereco:info.porta, pergunta INFO e desconecta (timeout de 2 s).
// Preenche info.jogadores, info.capacidade e info.nome (o servidor se identifica).
// Se o lobby estiver cheio, deixa jogadores == capacidade (estahCheio() fica true).
bool consultarServidor(ServidorInfo& info);
#endif
 
// Cliente com duas threads:
//  - thread principal: teclado / interface
//  - thread de recepção (interna): lê o socket e atualiza o estado

class ClienteTCP{

    // Lista imutável de jogadores do lobby. Cada mensagem LOBBY do servidor gera uma
    // lista nova (os Jogador nunca são alterados depois de criados), então copiar a
    // lista entre threads é seguro. O Jogador aqui só tem id, nome e pronto
    // (o cliente não conhece as cartas dos outros).
public:
    using ListaLobby = std::vector<std::shared_ptr<const Jogador>>;
    ClienteTCP(/* args */)=default;
    ~ClienteTCP();
;   // Defina ANTES de conectar() (a thread de recepção lê este callback).
    // É chamado pela thread de recepção, sem nenhum lock travado.
    std::function<void()> aoAtualizar;//chamada quando o estado do lobby é atualizado, para atualizar a interface do usuário

    bool conectar(const std::string& host, int porta);
    bool enviar(const std::string& linha);  // acrescenta '\n'
    bool esperarNome();                     // true = aceito; false = inválido ou desconectou
    void fechar();
 
    ListaLobby lobby() const;
    int meuId() const;// id do jogador no lobby (0 se não conectado)
    bool estouPronto() const;// true se o jogador marcou como pronto
    bool conectado() const { return ativo; }// true se o socket está aberto e a thread de recepção está rodando
    bool iniciou() const { return comecou; }// true se o servidor enviou a mensagem "INICIAR" (o jogo começou)

   
    std::string obterEstadoMesaLocal();
    void esperarMesa();
    int obterTurnoAtual();
    void setTurnoAtual(int t);

    std::atomic<bool> aguardandoNao{false};

    bool estaAguardandoNao() const { return aguardandoNao; }
    void setAguardandoNao(bool v) { aguardandoNao = v; }
 
    bool estaAguardandoOutrosReagirem() const;
    void setAguardandoOutrosReagirem(bool v);
private:
    void receber();// thread de recepção: lê o socket e atualiza o estado do lobby
    void tratarLinha(const std::string& linha);// processa uma linha recebida do servidor (LOBBY, INICIAR, ERRO, etc.)
 
    int fd = -1;// socket do cliente
    std::thread thRecepcao;// thread de recepção (receber())
    std::mutex mtxEnvio;  // evita duas threads misturando send()
 
    mutable std::mutex mtx;  // protege o estado abaixo
    std::condition_variable cv;// acorda quem espera a resposta do servidor (nome aceito ou não)
    ListaLobby jogadoresDoLobby;// lista de jogadores do lobby (imutável, cada LOBBY gera uma nova)
    int id = 0;// id do jogador no lobby (0 se não conectado)
    bool nomeAceito = false;
    bool erroNome = false;// true se o servidor respondeu "ERRO NOME_INVALIDO"
 
    std::atomic<bool> ativo{false};// true se o socket está aberto e a thread de recepção está rodando
    std::atomic<bool> comecou{false};// true se o servidor enviou a mensagem "INICIAR" (o jogo começou)
    std::string estadoMesaAtual;
    bool mesaAtualizada = false;
    bool aguardandoMinhaCarta{false};

    int turnoAtual = -1;
    
/*
void conectar(const std::string& ip, int port);

void enviar(const std::string& mensagem);

std::string receber();

void fechar();
*/
};


