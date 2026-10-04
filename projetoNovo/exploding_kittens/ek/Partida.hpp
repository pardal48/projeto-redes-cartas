#pragma once
// Partida.hpp - regras do Exploding Kittens (turnos, baralho, pilha de efeitos, reação com "Não").
//
// THREAD-SAFETY: a Partida NÃO tem mutex próprio. O ServidorTCP só a chama com o seu mtx travado
// (recv-threads, thread do relógio e removerCliente), o que serializa tudo. As funções de envio
// (servidor.enviarTudo / servidor.broadcast) também NÃO travam o mtx: já está travado.
//
// TIMER DO "NÃO" NÃO-BLOQUEANTE: nada aqui dorme. A Partida guarda um prazo (proximoPrazo) e a
// thread do relógio do servidor chama processarTempo() quando ele vence.
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "Carta.hpp"
#include "Jogador.hpp"

// Duração da janela do "Não" e do tempo máximo para responder Favor / reinserir bomba.
// Podem ser sobrescritas na compilação (-DEK_JANELA_NAO_MS=300) para os testes automáticos.
#ifndef EK_JANELA_NAO_MS
#define EK_JANELA_NAO_MS 5000
#endif
#ifndef EK_RESPOSTA_MS
#define EK_RESPOSTA_MS 30000
#endif

class ServidorTCP;
struct ClienteConectado;

class Partida {
public:
    using Relogio = std::chrono::steady_clock;
    using Instante = Relogio::time_point;

    explicit Partida(std::vector<std::shared_ptr<ClienteConectado>>& listaClientes);
    ~Partida();

    void iniciar(ServidorTCP& servidor);
    // Comandos de jogo: MESA, MAO, COMPRAR, JOGAR, JOGAR_NAO, DAR, POSICAO
    void processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor);
    // Morte por explosão (desconexao=false) ou saída/queda de um jogador (desconexao=true).
    void removerJogador(int idCliente, bool desconexao, ServidorTCP& servidor);
    // Executa o que estiver vencido (fim da janela do Não, timeouts de Favor/bomba).
    void processarTempo(ServidorTCP& servidor, Instante agora);
    // Próximo instante em que processarTempo() terá trabalho; nullopt se não há prazo.
    std::optional<Instante> proximoPrazo() const;

    bool estaEmAndamento() const { return emAndamento; }
    bool terminou() const { return terminada; }
    std::string obterEstadoMesa(const ClienteConectado& cliente) const;

    // ---- API usada pelos efeitos das cartas (Carta::aplicarEfeito) ----
    void encerrarTurnoSemComprar();   // Pular
    void atacarProximoJogador();      // Atacar
    void embaralharBaralho();         // Embaralhar
    void mostrarFuturo();             // Ver o futuro
    void iniciarFavor(std::shared_ptr<ClienteConectado> alvo);  // Favor

private:
    enum class Fase { TurnoNormal, JanelaNao, EscolhendoFavor, ReinserindoBomba };

    // comandos
    void cmdJogar(ClienteConectado& cliente, const std::vector<std::string>& t);
    void cmdJogarNao(ClienteConectado& cliente, const std::vector<std::string>& t);
    void cmdComprar(ClienteConectado& cliente, const std::vector<std::string>& t);
    void cmdDar(ClienteConectado& cliente, const std::vector<std::string>& t);
    void cmdPosicao(ClienteConectado& cliente, const std::vector<std::string>& t);

    // mecânica
    void resolverJanela();            // fim da janela: aplica ou cancela a pilha de efeitos
    void comprarCarta(ClienteConectado& cliente);
    void concluirFavor(std::size_t indiceNaMaoDoAlvo);
    void reinserirBomba(std::size_t posicaoDoTopo);
    void roubarAleatoria(int idLadrao, std::shared_ptr<ClienteConectado> vitima);
    void roubarEspecifica(int idLadrao, std::shared_ptr<ClienteConectado> vitima, TipoCarta pedido);
    bool transferir(ClienteConectado& de, ClienteConectado& para, std::size_t indice, const char* origem);
    void encerrarUmTurno();
    void passarTurno(int turnos, bool porAtaque);
    void verificarFimDeJogo();
    bool removerBombaAleatoria();
    void descartar(std::vector<std::unique_ptr<Carta>>& cartas);

    // utilidades
    std::unique_ptr<Carta> novaCarta(TipoCarta tipo);
    std::shared_ptr<ClienteConectado> buscarPorId(int id) const;
    std::shared_ptr<ClienteConectado> clienteDaVez() const;
    int idDaVez() const;
    const char* faseToken() const;
    void enviar(ClienteConectado& c, const std::string& msg);
    void transmitir(const std::string& msg);
    void enviarMao(ClienteConectado& c);
    void anunciarTurno();

    // Cópia dos shared_ptr (NÃO referência à lista do servidor): o servidor remove clientes da sua
    // lista ao desconectarem, e 'ordemTurnos' guarda ÍNDICES neste vetor, que precisam ser estáveis.
    std::vector<std::shared_ptr<ClienteConectado>> jogadores;
    ServidorTCP* srv = nullptr;
    std::mt19937 rng;

    bool emAndamento = false;
    bool terminada = false;
    int numeroJogadoresVivos = 0;
    std::deque<int> ordemTurnos;        // lista circular de turnos: índices em 'jogadores'; frente = jogador da vez
    std::vector<std::unique_ptr<Carta>> baralho;       // topo = back()
    std::vector<std::unique_ptr<Carta>> pilhaDescarte;
    std::vector<std::unique_ptr<Carta>> pilhaEfeitos;  // carta(s) da ação pendente + os "Não" jogados sobre ela
    int turnosPendentes = 1;            // turnos que o jogador da vez ainda deve cumprir (inclui o atual)
    bool porAtaque = false;             // esses turnos vieram de um Atacar?
    int proximoIdCarta = 1;

    Fase fase = Fase::TurnoNormal;
    Instante prazo{};                   // vale para JanelaNao, EscolhendoFavor e ReinserindoBomba

    // ação pendente (fase JanelaNao)
    int acaoJogadorId = 0;
    std::shared_ptr<ClienteConectado> acaoAlvo;
    TipoCarta acaoPedido = TipoCarta::Pular;
    std::size_t qtdCartasAcao = 0;      // quantas cartas de pilhaEfeitos são a ação (o resto são "Não")

    // Favor pendente (fase EscolhendoFavor)
    int favorSolicitanteId = 0;
    int favorAlvoId = 0;

    // bomba em reinserção (fase ReinserindoBomba)
    int bombaJogadorId = 0;
    std::unique_ptr<Carta> bombaEmMao;
    bool descartarBombaReinserida = false;
};
