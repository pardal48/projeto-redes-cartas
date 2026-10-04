#include "Partida.hpp"
#include "Carta.hpp"
#include "Jogador.hpp"
#include "ServidorTCP.hpp"
#include "Conexao.hpp" // Supondo que ClienteConectado esteja definido aqui ou no ServidorTCP.hpp

#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>

// Construtor
Partida::Partida(std::vector<std::shared_ptr<ClienteConectado>>& listaClientes) 
    : jogadores(listaClientes) {}

// Inicialização e Setup do Jogo
void Partida::iniciar(ServidorTCP& servidor) {
    std::lock_guard<std::mutex> lock(mtxJogo);
    
    if (jogadores.size() < 2 || jogadores.size() > 5) {
        servidor.broadcast("ERRO NUMERO_JOGADORES_INVALIDO\n");
        return;
    }

    emAndamento = true;
    numeroJogadoresVivos = jogadores.size();
    
    // Configura a ordem dos turnos
    ordemTurnos.clear();
    for (size_t i = 0; i < jogadores.size(); ++i) {
        ordemTurnos.push_back(i); 
    }

    // 1. Cria o baralho base (sem defuses e sem kittens)[cite: 8, 9]
    std::vector<std::shared_ptr<Carta>> baralhoBase;
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(1, "ATACAR", "Termina turno e prox joga 2x"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(2, "PULAR", "Termina turno sem comprar"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(3, "FAVOR", "Pede carta"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(4, "EMBARALHAR", "Embaralha"));
    for(int i=0; i<5; i++) baralhoBase.push_back(std::make_shared<Carta>(5, "FUTURO", "Ve 3 cartas"));
    for(int i=0; i<5; i++) baralhoBase.push_back(std::make_shared<Carta>(6, "NAO", "Cancela acao"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(7, "GATO1", "Gato normal"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(8, "GATO2", "Gato normal"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(9, "GATO3", "Gato normal"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(10, "GATO4", "Gato normal"));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_shared<Carta>(11, "GATO5", "Gato normal"));

    // Embaralha o baralho base
    auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(baralhoBase.begin(), baralhoBase.end(), rng);

    // 2. Distribui as cartas iniciais (1 Defuse + 7 aleatórias)[cite: 8]
    for (auto& c : jogadores) {
        c->jogador.adicionarCartaMao(std::make_shared<Carta>(0, "DEFUSE", "Salva da bomba"));
        for (int i = 0; i < 7; ++i) {
            c->jogador.adicionarCartaMao(baralhoBase.back());
            baralhoBase.pop_back();
        }
    }

    // 3. Adiciona Exploding Kittens (jogadores - 1) e Defuses extras (até 2)[cite: 8]
    int numKittens = jogadores.size() - 1;
    for(int i=0; i<numKittens; i++) {
        baralhoBase.push_back(std::make_shared<Carta>(99, "BOMBA", "Exploding Kitten"));
    }
    
    int defusesExtras = (jogadores.size() == 5) ? 1 : 2;
    for(int i=0; i<defusesExtras; i++) {
        baralhoBase.push_back(std::make_shared<Carta>(0, "DEFUSE", "Salva da bomba"));
    }

    // Embaralha o baralho final[cite: 8]
    baralho = baralhoBase;
    std::shuffle(baralho.begin(), baralho.end(), rng);

    servidor.broadcast("JOGO_INICIADO\n");
    servidor.broadcast("TURNO " + std::to_string(jogadores[ordemTurnos.front()]->jogador.getId()) + "\n");
}

// Roteador de Comandos
void Partida::processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor) {
    std::istringstream iss(comando);
    std::string acao;
    iss >> acao;

    if (acao == "MESA") {
        std::lock_guard<std::mutex> lock(mtxJogo);
        std::string status = obterEstadoMesa(cliente);
        servidor.enviarTudo(cliente.socket, status);
        return;
    }

    if (acao == "NAO") {
        reagirComNao(cliente, servidor); // O Não tem mecânica própria de concorrência[cite: 9]
        return;
    }

    // Comandos de turno exigem bloqueio e validação de turno
    std::lock_guard<std::mutex> lock(mtxJogo);
    if (!emAndamento) return;

    int idxAtual = ordemTurnos.front();
    if (jogadores[idxAtual]->jogador.getId() != cliente.jogador.getId()) {
        servidor.enviarTudo(cliente.socket, "ERRO NAO_E_SEU_TURNO\n");
        return;
    }

    if (aguardandoReacao) {
        servidor.enviarTudo(cliente.socket, "ERRO AGUARDANDO_REACOES\n");
        return;
    }

    if (acao == "COMPRAR") {
        comprarCarta(cliente, servidor);
    } else if (acao == "JOGAR") {
        int idxCarta, idAlvo = -1;
        iss >> idxCarta;
        if (iss >> idAlvo) { /* Alvo lido */ }
        jogarCarta(cliente, idxCarta, idAlvo, servidor);
    } else {
        servidor.enviarTudo(cliente.socket, "ERRO COMANDO_INVALIDO\n");
    }
}

// Ação de comprar carta[cite: 8]
void Partida::comprarCarta(ClienteConectado& cliente, ServidorTCP& servidor) {
    if (baralho.empty()) return;

    auto carta = baralho.back();
    baralho.pop_back();

    if (carta->getNome() == "BOMBA") {
        servidor.broadcast("EXPLOSAO " + std::to_string(cliente.jogador.getId()) + "\n");
        
        bool defusou = false;
        auto mao = cliente.jogador.getMao();
        for (size_t i = 0; i < mao.size(); ++i) {
            if (mao[i]->getNome() == "DEFUSE") {
                cliente.jogador.removerCartaMao(i);
                pilhaDescarte.push_back(mao[i]);
                defusou = true;
                break;
            }
        }

        if (defusou) {
            // Em uma interface CLI real, o jogador escolheria o local. 
            // Para simplificar, colocamos a bomba em uma posição aleatória do baralho[cite: 8]
            auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> dist(0, baralho.size());
            baralho.insert(baralho.begin() + dist(rng), carta);
            servidor.broadcast("DEFUSOU " + std::to_string(cliente.jogador.getId()) + "\n");
        } else {
            removerJogador(cliente.jogador.getId(), false, servidor);
        }
    } else {
        cliente.jogador.adicionarCartaMao(carta);
        servidor.enviarTudo(cliente.socket, "COMPROU " + carta->getNome() + "\n");
    }

    if (emAndamento) {
        passarTurno(servidor);
    }
}

// Lógica de gerenciar turnos pendentes[cite: 9]
void Partida::passarTurno(ServidorTCP& servidor) {
    turnosPendentes--;
    if (turnosPendentes <= 0) {
        // Passa pro próximo da fila
        int atual = ordemTurnos.front();
        ordemTurnos.pop_front();
        ordemTurnos.push_back(atual);
        turnosPendentes = 1;
    }
    servidor.broadcast("TURNO " + std::to_string(jogadores[ordemTurnos.front()]->jogador.getId()) + "\n");
}

// Reação com a Carta "Não"[cite: 9]
void Partida::reagirComNao(ClienteConectado& cliente, ServidorTCP& servidor) {
    std::lock_guard<std::mutex> lock(mtxJogo);
    if (!aguardandoReacao) {
        servidor.enviarTudo(cliente.socket, "ERRO NENHUMA_ACAO_PARA_CANCELAR\n");
        return;
    }

    auto mao = cliente.jogador.getMao();
    int idxNao = -1;
    for (size_t i = 0; i < mao.size(); ++i) {
        if (mao[i]->getNome() == "NAO") {
            idxNao = i; break;
        }
    }

    if (idxNao != -1) {
        pilhaEfeitos.push_back(mao[idxNao]);
        cliente.jogador.removerCartaMao(idxNao);
        servidor.broadcast("JOGOU_NAO " + std::to_string(cliente.jogador.getId()) + "\n");
    } else {
        servidor.enviarTudo(cliente.socket, "ERRO VOCE_NAO_TEM_A_CARTA_NAO\n");
    }
}

void Partida::jogarCarta(ClienteConectado& cliente, int indiceCarta, int idAlvo, ServidorTCP& servidor) {
    auto mao = cliente.jogador.getMao();
    if (indiceCarta < 0 || static_cast<size_t>(indiceCarta) >= mao.size()) {
        servidor.enviarTudo(cliente.socket, "ERRO INDICE_INVALIDO\n");
        return;
    }

    auto cartaJogada = mao[indiceCarta];
    std::string nomeCarta = cartaJogada->getNome();

    // Remove da mão e joga na pilha de descarte/efeito
    cliente.jogador.removerCartaMao(indiceCarta);
    
    // Se não for uma carta de reação instantânea, aguarda o timer do NÃO[cite: 9]
    if (nomeCarta != "DEFUSE" && nomeCarta != "BOMBA" && nomeCarta != "NAO") {
        aguardandoReacao = true;
        pilhaEfeitos.push_back(cartaJogada);
        servidor.broadcast("JOGOU " + std::to_string(cliente.jogador.getId()) + " " + nomeCarta + "\n");
        
        // Timer de 5 segundos rodando de forma assíncrona
        std::thread([this, &servidor, nomeCarta, idAlvo]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            this->aplicarEfeitosPendentes(servidor);
        }).detach();
    }
}

void Partida::aplicarEfeitosPendentes(ServidorTCP& servidor) {
    std::lock_guard<std::mutex> lock(mtxJogo);
    if (!aguardandoReacao || pilhaEfeitos.empty()) return;

    // Se o número de "NÃO" for par, o efeito original (index 0) é cancelado[cite: 9]
    bool cancelado = (pilhaEfeitos.size() % 2 == 0);
    auto cartaEfeito = pilhaEfeitos.front();

    // Joga tudo pro descarte
    for (auto& c : pilhaEfeitos) pilhaDescarte.push_back(c);
    pilhaEfeitos.clear();
    aguardandoReacao = false;

    if (cancelado) {
        servidor.broadcast("CANCELADO\n");
        return;
    }

    std::string nome = cartaEfeito->getNome();
    if (nome == "ATACAR") {
        // Encerra turno sem comprar, prox joga o restante dos turnos atuais + 2[cite: 9]
        int turnosAtuais = turnosPendentes; 
        turnosPendentes = 0; // zera para pular o turno do atacante
        passarTurno(servidor); // passa pro alvo
        turnosPendentes = turnosAtuais + 1; // o alvo fica com os turnos do atacante (se empilhou) + 2[cite: 9]
    } 
    else if (nome == "PULAR") {
        turnosPendentes--;
        if (turnosPendentes <= 0) passarTurno(servidor);[cite: 9]
    }
    else if (nome == "EMBARALHAR") {
        auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
        std::shuffle(baralho.begin(), baralho.end(), rng);
        servidor.broadcast("EMBARALHOU\n");[cite: 9]
    }
    // ... Implementação dos combos de Gato e Favor entrariam na mesma lógica aqui, lidando com o idAlvo[cite: 9].
}

void Partida::removerJogador(int idCliente, bool desconexao, ServidorTCP& servidor) {
    auto it = std::find_if(ordemTurnos.begin(), ordemTurnos.end(), [this, idCliente](int idx) {
        return jogadores[idx]->jogador.getId() == idCliente;
    });

    if (it != ordemTurnos.end()) {
        int idx = *it;
        ordemTurnos.erase(it);
        numeroJogadoresVivos--;

        if (desconexao) {
            // Regra: se desconecta vivo, cartas pro baralho e remove defuse e uma bomba[cite: 8]
            auto mao = jogadores[idx]->jogador.getMao();
            for (auto& c : mao) {
                if (c->getNome() != "DEFUSE") baralho.push_back(c);
            }
            // Remove aleatoriamente uma bomba para compensar
            for (auto bit = baralho.begin(); bit != baralho.end(); ++bit) {
                if ((*bit)->getNome() == "BOMBA") {
                    baralho.erase(bit); break;
                }
            }
            auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
            std::shuffle(baralho.begin(), baralho.end(), rng);
        } else {
            // Morreu por explosão
            auto mao = jogadores[idx]->jogador.getMao();
            for (auto& c : mao) pilhaDescarte.push_back(c);
        }

        servidor.broadcast("MORREU " + std::to_string(idCliente) + "\n");
        verificarFimDeJogo(servidor);
    }
}

void Partida::verificarFimDeJogo(ServidorTCP& servidor) {
    if (numeroJogadoresVivos <= 1) {
        emAndamento = false;
        if (!ordemTurnos.empty()) {
            int vencedorId = jogadores[ordemTurnos.front()]->jogador.getId();
            servidor.broadcast("FIM_DE_JOGO VENCEDOR " + std::to_string(vencedorId) + "\n");
        }
    }
}

std::string Partida::obterEstadoMesa(const ClienteConectado& cliente) const {
    std::ostringstream oss;
    oss << "STATUS JOGADORES_VIVOS:" << numeroJogadoresVivos 
        << " BARALHO:" << baralho.size() 
        << " DESCARTE:" << pilhaDescarte.size() << "\n";
    return oss.str();
}