#pragma once
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Carta.hpp"
#include "Conexao.hpp"

class ServidorTCP;

// Regras e estado de uma partida de Exploding Kittens.
// Todos os métodos públicos são chamados com o mutex do servidor já travado.
//
// Protocolo servidor -> cliente gerado aqui:
//   TURNO <id>               de quem é a vez (sempre seguido de MESA_ESTADO)
//   MESA_ESTADO <...>        estado da mesa, personalizado para cada jogador
//   JOGOU <id> <carta>       carta jogada (abre a janela de reação)
//   PERGUNTA_NAO <id> <carta>  pergunta privada: quer jogar NAO?
//   JOGOU_NAO <id> | CANCELADO | EMBARALHOU | FUTURO <a> <b> <c>
//   COMPROU <id> | COMPROU_VOCE <carta>
//   EXPLOSAO <id> | DEFUSOU <id> | MORREU <id> | FIM_DE_JOGO VENCEDOR <id>
class Partida {
public:
    using ListaClientes = std::vector<std::shared_ptr<ClienteConectado>>;

    explicit Partida(const ListaClientes& clientes);
    ~Partida();

    void iniciar(ServidorTCP& servidor);
    void processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor);
    void removerJogador(int idJogador, ServidorTCP& servidor);  // desconexão
    bool emAndamento() const { return andamento; }

private:
    using Baralho = std::vector<std::unique_ptr<Carta>>;
    enum class Motivo { Explosao, Desconexao };
    enum class tipoDoacao { Favor, combo2, combo3, nothing };

    // ---- preparação ----
    void montarBaralho();

    // ---- turnos ----
    void anunciarTurno(ServidorTCP& servidor);  // TURNO + mesa, UMA vez por mudança de estado
    void consumirTurno();                       // gasta um turno pendente; passa a vez se acabaram
    void passarParaProximo();

    // ---- ações do jogador da vez ----
    void comprarCarta(ClienteConectado& cliente, ServidorTCP& servidor);
    void jogarCarta(ClienteConectado& cliente, int indice, ServidorTCP& servidor);
    void jogarCombo(ClienteConectado& cliente, std::istringstream& cartas, ServidorTCP& servidor);

    // ---- janela de reação (cartas NAO) ----
    void abrirJanelaReacao(ServidorTCP& servidor);
    void perguntarAoPrimeiro(ServidorTCP& servidor);
    void processarRespostaNao(ClienteConectado& cliente, bool querJogar, ServidorTCP& servidor);
    void resolverEfeitos(ServidorTCP& servidor);
    void aplicarEfeito(TipoCarta tipo, ServidorTCP& servidor);
    void transferirCarta(int posicaoCarta, std::shared_ptr<ClienteConectado> origem, std::shared_ptr<ClienteConectado> destino);
    void processarRoubarCarta(std::shared_ptr<ClienteConectado> alvo, ServidorTCP& servidor);
    void descartarEfeitosPendentes();

    // ---- eliminação / fim ----
    void eliminarJogador(int idJogador, Motivo motivo, ServidorTCP& servidor);
    bool verificarFimDeJogo(ServidorTCP& servidor);
    void removerUmaBombaDoBaralho();

    // ---- utilidades ----
    std::shared_ptr<ClienteConectado> obterJogadorPorId(int idJogador) const;
    std::string obterEstadoMesa(const ClienteConectado& cliente) const;
    void notificarTodosMesa(ServidorTCP& servidor);
    static void enviar(const ClienteConectado& cliente, const std::string& msg);

    ListaClientes jogadores;          // todos os que começaram a partida (vivos ou não)
    std::mt19937 rng;

    Baralho baralho;                  // topo = back()
    Baralho pilhaDescarte;
    Baralho pilhaEfeitos;             // carta jogada + NAOs empilhados, aguardando resolução

    std::deque<int> ordemTurnos;      // só jogadores vivos; front() = jogador da vez
    std::deque<int> filaRespostaNao;  // quem ainda precisa responder à pergunta do NAO
    int turnosPendentes = 1;          // turnos que o jogador da vez ainda tem que jogar
    int idUltimoAutor = -1;           // autor da carta (ou NAO) que está no topo da pilha de efeitos
    int idJogadorDoador = -1;
    bool aguardandoReacao = false;
    bool aguardandoEscolhaOponente = false;  // carta Favor ou combos
    bool aguardandoEscolhaCarta = false;     // carta Favor
    enum tipoDoacao tipoDoacao = tipoDoacao::nothing;
    bool aguardandoEscolhaTipoCarta = false; // combo 3
    bool andamento = false;
};