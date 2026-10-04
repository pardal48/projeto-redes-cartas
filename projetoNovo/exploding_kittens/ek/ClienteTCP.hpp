#pragma once
// ClienteTCP.hpp - conexão do cliente: thread de recepção + estado do lobby.
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Jogador.hpp"

// Abre uma conexão TCP IPv4. Devolve o fd ou -1.
int conectarTCP(const std::string& host, int porta, bool verboso = true);

class ClienteTCP {
public:
    using ListaLobby = std::vector<std::shared_ptr<Jogador>>;

    ClienteTCP() = default;
    ~ClienteTCP();
    ClienteTCP(const ClienteTCP&) = delete;
    ClienteTCP& operator=(const ClienteTCP&) = delete;

    bool conectar(const std::string& host, int porta);
    bool enviar(const std::string& linha);   // acrescenta '\n'; thread-safe
    bool esperarNome();                      // bloqueia até o servidor aceitar/rejeitar o nome
    void fechar();

    ListaLobby lobby() const;                // cópia do lobby (thread-safe)
    int meuId() const;
    bool estouPronto() const;
    bool conectado() const { return ativo; }
    bool iniciou() const;                    // recebeu INICIAR
    bool partidaTerminou() const;            // recebeu FIM_DE_JOGO
    void resetarPartida();                   // volta ao estado de lobby (nova partida)
    // Consultas ao lobby (o servidor não manda LOBBY durante a partida, então os nomes não mudam).
    std::string nomeDoJogador(int idJogador) const;   // "#<id>" se desconhecido
    int idPorNome(const std::string& nome) const;     // -1 se não achar (sem distinguir maiúsculas)

    // Chamados na thread de recepção, SEMPRE fora do mutex interno.
    std::function<void()> aoAtualizar;                     // o lobby mudou
    std::function<void(const std::string&)> aoEvento;      // qualquer outra mensagem do servidor

private:
    void receber();                          // loop da thread de recepção
    void tratarLinha(const std::string& linha);

    int fd = -1;
    std::atomic<bool> ativo{false};
    std::thread thRecepcao;

    mutable std::mutex mtx;                  // protege o estado abaixo
    std::mutex mtxEnvio;                     // serializa send() de várias threads
    std::condition_variable cv;

    int id = -1;
    ListaLobby jogadoresDoLobby;
    bool nomeAceito = false;
    bool erroNome = false;
    bool comecou = false;
    bool fimDeJogo = false;
};
