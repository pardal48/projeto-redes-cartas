#pragma once
// ServidorTCP.hpp - camada de rede do servidor + lobby. As regras do jogo ficam em Partida.
//
// CONCORRÊNCIA
//   * Uma thread aceita conexões (executar), uma thread por cliente faz recv (atenderCliente)
//     e uma thread "relógio" controla a contagem do lobby e os prazos da Partida.
//   * UM único mutex (mtx) protege: clientes, jogoIniciado, contagem do lobby e a Partida inteira.
//     Ninguém dorme com o mtx travado: o relógio espera em condition_variable::wait_until (que
//     libera o mtx) e é acordado com cv.notify_all() sempre que um prazo pode ter mudado.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Conexao.hpp"
#include "Jogador.hpp"

class Partida;

struct ClienteConectado {
    int socket = -1;   // vira -1 quando o servidor fecha a conexão (ninguém mais deve enviar a ele)
    Jogador jogador;
};

class ServidorTCP {
public:
    ServidorTCP(int port, const std::string& nome);
    ~ServidorTCP();
    ServidorTCP(const ServidorTCP&) = delete;
    ServidorTCP& operator=(const ServidorTCP&) = delete;

    void executar();      // loop de accept (poll com timeout de 500 ms)
    void parar();         // seguro para chamar de um signal handler
    int getporta() const;

    // Usados pela Partida (sempre com o mtx já travado pelo servidor; não travam de novo)
    bool enviarTudo(int fd, const std::string& msg);
    void broadcast(const std::string& msg);
    // Partida em andamento (usada pelos efeitos das cartas). Só chamar com jogoIniciado.
    Partida& partidaAtiva();

private:
    std::shared_ptr<ClienteConectado> registrar(int fd);
    void atenderCliente(std::shared_ptr<ClienteConectado> client);
    void removerCliente(const std::shared_ptr<ClienteConectado>& client);
    void encerrar();

    // lobby e jogo
    void processarLinha(ClienteConectado& client, const std::string& linha);
    void processarComandoJogo(ClienteConectado& client, const std::string& linha);
    void verificarFimDeJogo();           // se a Partida acabou: volta todo mundo ao lobby
    bool nomeValido(const std::string& nome) const;
    bool nomeEmUso(const std::string& nome) const;
    bool todosProntos() const;
    size_t jogadoresNoLobby() const;
    std::string estadoComoTexto() const;

    // contagem regressiva do lobby e relógio
    void reavaliarContagem(bool houveMudanca);
    void iniciarContagem();
    void cancelarContagem();
    void avancarContagem();
    void iniciarPartida();
    void relogio();

    int porta;
    std::string nomeServidor;
    int servidorSocket = -1;
    ServidorInfo info;
    std::atomic<bool> rodando{false};

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::shared_ptr<ClienteConectado>> clientes;
    std::vector<std::thread> threads;
    std::thread threadRelogio;
    int proximoId = 1;       // ids de jogador começam em 1 (0 significa "ninguém" no protocolo)
    bool jogoIniciado = false;
    std::unique_ptr<Partida> partidaAtual;

    bool contagemAtiva = false;
    int contagemValor = 0;
    std::chrono::steady_clock::time_point proximoTick;
};
