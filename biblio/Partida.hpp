#ifndef PARTIDA_HPP
#define PARTIDA_HPP
#include "Carta.hpp"
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
// Forward declarations para evitar dependências circulares
class Carta;
struct ClienteConectado; 
class ServidorTCP;

class Partida {
private:
    // --- Estruturas de Cartas ---
    std::vector<std::unique_ptr<Carta>> baralho;
    std::vector<std::unique_ptr<Carta>> pilhaDescarte;
   
    
    // --- Estruturas de Clientes (Jogadores) ---
    // Referência para a lista de clientes mantida pelo Servidor/Lobby
    std::vector<std::shared_ptr<ClienteConectado>>& jogadores;
    
    // Usamos deque para manter a ordem dos turnos (facilita pular ou remover quem morrer)
    std::deque<int> ordemTurnos; 

    // --- Estado do Jogo ---
    bool emAndamento{false};
    size_t indiceJogadorAtual{0};
    int numeroJogadoresVivos{0};
    int turnosPendentes{1}; // Essencial para gerenciar a carta "Atacar" (2 turnos)

    // --- Sistema de Efeitos e Timer ("Não") ---
    bool aguardandoReacao{false};
    std::vector<std::unique_ptr<Carta>> pilhaEfeitos;// Acumula os "Não" jogados em sequência[cite: 9]

    std::unordered_set<int> pendentesRespostaNao; // IDs dos jogadores que faltam responder

    // --- Métodos Internos Auxiliares ---
    //void distribuirCartas(ServidorTCP& servidor);
    //void embaralharBaralho();
    void aplicarEfeitosPendentes(ServidorTCP& servidor);
    
    void processarRespostaNao(ClienteConectado& cliente, bool querJogar, ServidorTCP& servidor);
public:
    // Construtor
    explicit Partida(std::vector<std::shared_ptr<ClienteConectado>>& listaClientes);
    ~Partida();
    void removerJogador(int idCliente, bool desconexao, ServidorTCP& servidor); // Trata mortes e desconexões
    // --- Fluxo Principal ---
    void iniciar(ServidorTCP& servidor);
    void processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor);
    void verificarFimDeJogo(ServidorTCP& servidor);
    void passarTurno(ServidorTCP& servidor);

    // --- Ações de Jogo ---
    // Separadas do processarComando para deixar o código mais limpo
    void comprarCarta(ClienteConectado& cliente, ServidorTCP& servidor); // Encerra o turno[cite: 8]
    void jogarCarta(ClienteConectado& cliente, int indiceCarta, int idAlvo, ServidorTCP& servidor);
    void reagirComNao(ClienteConectado& cliente, ServidorTCP& servidor);

    // --- Getters e Utilitários ---
    bool estaEmAndamento() const { return emAndamento; }
    std::string obterEstadoMesa(const ClienteConectado& cliente) const;
    std::shared_ptr<ClienteConectado>   obterJogadorPorId(int id);
};

#endif // PARTIDA_HPP