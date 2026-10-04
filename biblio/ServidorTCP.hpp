#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Conexao.hpp"
#include "Partida.hpp"

// Servidor TCP: aceita conexões, gerencia o lobby e delega as regras do jogo à Partida.
// Uma thread por cliente; o mutex 'mtx' protege todo o estado compartilhado.
class ServidorTCP {
public:
    ServidorTCP(int porta, const std::string& nome);
    ~ServidorTCP();
    ServidorTCP(const ServidorTCP&) = delete;
    ServidorTCP& operator=(const ServidorTCP&) = delete;

    bool ok() const { return servidorSocket != -1; }  // false se bind/listen falharam
    void executar();                                  // loop de accept até parar()
    void parar() { rodando = false; }                 // seguro dentro de signal handler
    int getporta() const { return porta; }

    void broadcast(const std::string& msg);           // chamado com mtx travado
    static bool enviarTudo(int fd, const std::string& msg);

private:
    std::shared_ptr<ClienteConectado> registrar(int fd);
    void atenderCliente(std::shared_ptr<ClienteConectado> client);
    void removerCliente(const std::shared_ptr<ClienteConectado>& client);
    void encerrar();

    void processarLinha(ClienteConectado& client, const std::string& linha);
    void verificarFimDaPartida();

    bool nomeValido(const std::string& nome) const;
    bool nomeEmUso(const std::string& nome) const;
    bool todosProntos() const;
    std::string estadoComoTexto() const;

    int porta;
    std::string nomeServidor;
    int servidorSocket = -1;
    std::atomic<bool> rodando{false};

    std::mutex mtx;
    std::vector<std::shared_ptr<ClienteConectado>> clientes;
    std::vector<std::thread> threads;
    int proximoId = 1;

    ServidorInfo info;
    bool jogoIniciado = false;
    std::unique_ptr<Partida> partidaAtual;
};