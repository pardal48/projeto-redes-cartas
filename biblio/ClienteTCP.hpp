#pragma once
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Jogador.hpp"

// O que o jogador local pode fazer agora (decide o prompt e como interpretar o teclado).
enum class ModoJogo {
    Aguardar,   // vez de outro jogador
    Reagir,     // alguém jogou uma carta: responder JOGAR_NAO ou PASSO
    MinhaVez,   // posso COMPRAR / JOGAR
    Eliminado,  // explodi: só assisto
    EscolherAlvo,   // meu FAVOR passou: digitar o nome de quem vai me dar uma carta
    EscolherCarta,   // alguém me pediu um FAVOR: digitar o número da carta a entregar
    EscolherTipoCarta // combo de 3 cartas: digitar o tipo da carta a ser roubada
};

// Abre uma conexão TCP; devolve o fd ou -1.
int conectarTCP(const std::string& host, int porta, bool verboso = true);

// Cliente de rede: uma thread de recepção interpreta as linhas do servidor,
// guarda o estado (lobby, mesa, turno) e avisa a interface por callbacks.
class ClienteTCP {
public:
    using ListaLobby = std::vector<std::shared_ptr<Jogador>>;

    // Fotos atômicas do estado; 'versao' sobe a cada atualização (a interface usa para não redesenhar igual).
    struct LobbySnapshot { unsigned versao; ListaLobby jogadores; };
    struct MesaSnapshot  { unsigned versao; std::string texto; };

    ClienteTCP() = default;
    ~ClienteTCP();
    ClienteTCP(const ClienteTCP&) = delete;
    ClienteTCP& operator=(const ClienteTCP&) = delete;

    // Defina os callbacks ANTES de conectar(): a thread de recepção os lê.
    std::function<void()> aoAtualizar;                              // lobby ou mesa mudou
    std::function<void(const std::string&, bool)> aoEvento;         // mensagem legível, ehErro

    bool conectar(const std::string& host, int porta);
    bool enviar(const std::string& linha);
    void fechar();

    // ---- conexão / nome ----
    bool conectado() const;
    bool esperarNome();   // bloqueia até o servidor aceitar/rejeitar o nome (false = rejeitado/desconectado)

    // ---- estado ----
    int meuId() const;
    bool estouPronto() const;
    bool iniciou() const;
    bool terminou() const;
    LobbySnapshot snapshotLobby() const;
    MesaSnapshot snapshotMesa() const;
    ModoJogo modoAtual() const;

    // Responde à pergunta do NAO (limpa o estado local antes de enviar, evitando corrida).
    void responderNao(bool jogarNao);

private:
    void receber();
    void tratarLinha(const std::string& linha);
    std::string nomeDe(int idJogador) const;  // mtx já travado

    int fd = -1;
    std::thread thRecepcao;
    mutable std::mutex mtx;      // estado abaixo
    std::mutex mtxEnvio;         // serializa send()
    std::condition_variable cv;

    bool ativo = false;
    int id = -1;
    bool nomeAceito = false;
    bool erroNome = false;
    bool comecou = false;
    bool fimDeJogo = false;
    bool vivo = true;

    ListaLobby jogadoresDoLobby;
    unsigned versaoLobby = 0;

    std::string estadoMesa;
    unsigned versaoMesa = 0;

    int turnoAtual = -1;
    bool aguardandoNao = false;        // o servidor me perguntou se quero jogar NAO
    bool aguardandoMinhaCarta = false; // joguei uma carta e os outros estão reagindo
    bool escolhendoAlvo = false;       // FAVOR: o servidor espera eu escolher o oponente
    bool escolhendoDoacao = false;     // FAVOR: o servidor espera eu escolher a carta a entregar
    bool aguardandoEscolhaTipoCarta = false; // combo de 3 cartas: digitar o tipo da carta a ser roubada
};