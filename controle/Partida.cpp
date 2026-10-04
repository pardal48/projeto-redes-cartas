#include "Partida.hpp"
#include "Carta.hpp"
#include "Jogador.hpp"
#include "ServidorTCP.hpp"
#include "Conexao.hpp" // Supondo que ClienteConectado esteja definido aqui ou no ServidorTCP.hpp
#include "Carta.hpp"
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
    
    
    if (jogadores.size() < 2 || jogadores.size() > 5) {
        servidor.broadcast("ERRO NUMERO_JOGADORES_INVALIDO\n");
        return;
    }

    emAndamento = true;
    numeroJogadoresVivos = jogadores.size();
    
    // Configura a ordem dos turnos
    ordemTurnos.clear();
    for (auto& c : jogadores) {
    ordemTurnos.push_back(c->jogador.getId()); // Guarda o ID, não o índice (i)
    }

    // 1. Cria o baralho base (sem defuses e sem kittens)[cite: 8, 9]
    std::vector<std::unique_ptr<Carta>> baralhoBase;
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(1, "ATACAR", "Termina turno e prox joga 2x",TipoCarta:: Ataque));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(2, "PULAR", "Termina turno sem comprar",TipoCarta::Pular));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(3, "FAVOR", "Pede carta",TipoCarta::Favor));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(4, "EMBARALHAR", "Embaralha",TipoCarta::Embaralhar));
    for(int i=0; i<5; i++) baralhoBase.push_back(std::make_unique<Carta>(5, "FUTURO", "Ve 3 cartas",TipoCarta::Futuro));
    for(int i=0; i<5; i++) baralhoBase.push_back(std::make_unique<Carta>(6, "NAO", "Cancela acao",TipoCarta::Nao));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(7, "GATO1", "Gato normal",TipoCarta::GatoAranha));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(8, "GATO2", "Gato normal",TipoCarta::GatoBarba));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(9, "GATO3", "Gato normal",TipoCarta::GatoBatata));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(10, "GATO4", "Gato normal",TipoCarta::GatoMelancia));
    for(int i=0; i<4; i++) baralhoBase.push_back(std::make_unique<Carta>(11, "GATO5", "Gato normal",TipoCarta::GatoTaco));

    // Embaralha o baralho base
    auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(baralhoBase.begin(), baralhoBase.end(), rng);

    // 2. Distribui as cartas iniciais (1 Defuse + 7 aleatórias)[cite: 8]
    for (auto& c : jogadores) {
        c->jogador.adicionarCartaMao(std::make_unique<Carta>(0, "DEFUSE", "Salva da bomba",TipoCarta::Desarme));
        for (int i = 0; i < 7; ++i) {
            c->jogador.adicionarCartaMao(std::move(baralhoBase.back()));
            baralhoBase.pop_back();
        }
    }

    // 3. Adiciona Exploding Kittens (jogadores - 1) e Defuses extras (até 2)[cite: 8]
    int numKittens = jogadores.size() - 1;
    for(int i=0; i<numKittens; i++) {
        baralhoBase.push_back(std::make_unique<Carta>(99, "BOMBA", "Exploding Kitten",TipoCarta::Bomba));
    }
    
    int defusesExtras = (jogadores.size() == 5) ? 1 : 2;
    for(int i=0; i<defusesExtras; i++) {
        baralhoBase.push_back(std::make_unique<Carta>(0, "DEFUSE", "Salva da bomba",TipoCarta::Desarme));
    }

    // Embaralha o baralho final[cite: 8]
    baralho = std::move(baralhoBase);
    std::shuffle(baralho.begin(), baralho.end(), rng);

    servidor.broadcast("JOGO_INICIADO\n");
    servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");
    notificarTodosMesa(servidor);
}

// Roteador de Comandos
void Partida::processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor) {
    std::istringstream iss(comando);
    std::string acao;
    iss >> acao;

    if (acao == "MESA") {
        
        std::string status = obterEstadoMesa(cliente);
        servidor.enviarTudo(cliente.socket, status);
        return;
    }

    // Comandos de turno exigem bloqueio e validação de turno
    
    if (!emAndamento) return;
    
    //se outro jogador tiver jogado uma carta no turno dele
    if (aguardandoReacao) {
        if (acao == "JOGAR_NAO") {
            processarRespostaNao(cliente, true, servidor);
        } else if (acao == "PASSO") {
            processarRespostaNao(cliente, false, servidor);
        } else {
            servidor.enviarTudo(cliente.socket, "ERRO AGUARDANDO_RESPOSTA_NAO\n");
        }
        return;
    }
    
    // Valida se é a vez do jogador para as ações do turno normal
    int idAtual = ordemTurnos.front(); // Agora é diretamente o ID!
    if (idAtual != cliente.jogador.getId()) {
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
        if(!(iss >> idxCarta)){
            servidor.enviarTudo(cliente.socket,"ERRO COMANDO_INVALIDO_USE_NUMEROS\n");
            return;
        }
        if (iss >> idAlvo) { /* Alvo lido */ }
        jogarCarta(cliente, idxCarta, idAlvo, servidor);
    } else {
        servidor.enviarTudo(cliente.socket, "ERRO COMANDO_INVALIDO\n");
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
    servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");
    notificarTodosMesa(servidor);
}

void Partida::processarRespostaNao(ClienteConectado& cliente, bool querJogar, ServidorTCP& servidor) {
    if(!aguardandoReacao) return;
    int id = cliente.jogador.getId();
    idAutorUltimaCarta = id;
    if (filaRespostaNao.front() != id) {
        servidor.enviarTudo(cliente.socket, "ERRO NAO_E_SUA_VEZ_DE_REAGIR\n");
        return;
    }

    if (querJogar) {
        // Procura se o jogador realmente possui a carta NÃO
        int idxNao = -1;
        const auto& mao = cliente.jogador.getMao();
        for (size_t i = 0; i < mao.size(); ++i) {
            if (mao[i]->getTipo() == TipoCarta::Nao) {
                idxNao = static_cast<int>(i);
                break;
            }
        }
        
        if (idxNao != -1) {
            auto cartaNao = cliente.jogador.removerCartaMao(idxNao);
            pilhaEfeitos.push_back(std::move(cartaNao));
            servidor.broadcast("JOGOU_NAO " + std::to_string(id) + "\n");
            
            // Novo ciclo: quando um "NÃO" é jogado, todos precisam responder novamente para ver se jogam outro "NÃO"
            filaRespostaNao.clear();
            for (int idAtivo : ordemTurnos) {
                if (idAtivo != id) {
                    filaRespostaNao.push_back(idAtivo);
                } 
            }
            if (filaRespostaNao.empty()) {
                aplicarEfeitosPendentes(servidor);
            } else {
                servidor.broadcast("PERGUNTA_NAO " + std::to_string(id) + " NAO\n");
            }
            return;
        } else {
            servidor.enviarTudo(cliente.socket, "ERRO VOCE_NAO_TEM_A_CARTA_NAO\n");
            // Trata como PASSO se o jogador tentou enganar o servidor sem ter a carta
        }
    }
    
    // Registra a passagem/resposta do jogador
    filaRespostaNao.pop_front();
    
    // Se todos responderam, resolve a pilha na thread principal
    if (filaRespostaNao.empty()) {
        aplicarEfeitosPendentes(servidor);
    }else {
        // Pergunta para o próximo jogador da fila
        int idAutorAtual = pilhaEfeitos.empty() ? -1 : 1; // Ou o ID de quem jogou a última carta
        auto alvo = obterJogadorPorId(filaRespostaNao.front());
        if (alvo) {
            servidor.enviarTudo(alvo->socket, "PERGUNTA_NAO " + std::to_string(idAutorAtual) + " CARTA\n");
        }
    }
}

void Partida::jogarCarta(ClienteConectado& cliente, int indiceCarta, int /*idAlvo*/, ServidorTCP& servidor) {
    //auto mao = cliente.jogador.getMao();
    idAutorUltimaCarta = cliente.jogador.getId();
    if (indiceCarta < 0 || static_cast<size_t>(indiceCarta) >= cliente.jogador.getTamanhoMao()) {
        servidor.enviarTudo(cliente.socket, "ERRO INDICE_INVALIDO\n");
        return;
    }

    auto cartaJogada = cliente.jogador.removerCartaMao(indiceCarta);
    TipoCarta tipo = cartaJogada->getTipo();

    // Remove da mão e joga na pilha de descarte/efeito
    if (tipo == TipoCarta::Desarme || tipo == TipoCarta::Bomba || tipo == TipoCarta::Nao) {
        cliente.jogador.adicionarCartaMao(std::move(cartaJogada)); // Devolve pra mão
        servidor.enviarTudo(cliente.socket, "ERRO CARTA_DE_REACAO_NAO_PODE_SER_JOGADA_ASSIM\n");
        return;
    }
    //aqui da pra tirar o if do se não for desarme bomba ou nao
    // Se não for uma carta de reação instantânea, aguarda o timer do NÃO[cite: 9]
    if (tipo != TipoCarta::Desarme && tipo != TipoCarta::Bomba  && tipo != TipoCarta::Nao) {
        int idQuemJogou=cliente.jogador.getId();
        
        std::string nomeCarta =(*cartaJogada).getNome();
        pilhaEfeitos.push_back(std::move(cartaJogada));
        servidor.broadcast("JOGOU " + std::to_string(cliente.jogador.getId()) + " " + nomeCarta+ "\n");
        aguardandoReacao = true;
        filaRespostaNao.clear();

        // Solicita resposta de TODOS os jogadores vivos para não revelar a mão de ninguém
        for (int idAtivo : ordemTurnos) {
            if(idAtivo != idQuemJogou) filaRespostaNao.push_back(idAtivo);//não coloca que jogou a carta com efeito
        }
        
    
        // Em jogarCarta, altere o bloco final if (!filaRespostaNao.empty()):
        if (!filaRespostaNao.empty()) {
            auto alvo = obterJogadorPorId(filaRespostaNao.front());
            if (alvo) {
            servidor.enviarTudo(alvo->socket, "PERGUNTA_NAO " + std::to_string(idQuemJogou) + " " + nomeCarta + "\n");
            }
        } else {
            aplicarEfeitosPendentes(servidor);
        }
    }
}

void Partida::aplicarEfeitosPendentes(ServidorTCP& servidor) {
    
    if (!aguardandoReacao || pilhaEfeitos.empty()) return;
    //numero par de NAOs cancela acao original
    bool cancelado = (pilhaEfeitos.size() % 2 == 0);
    std::string nomeAcaoOriginal = pilhaEfeitos.front()->getNome();
    
    // Mover os ponteiros com std::move de forma limpa para a pilha de descarte
    while (!pilhaEfeitos.empty()) {
        pilhaDescarte.push_back(std::move(pilhaEfeitos.back()));
        pilhaEfeitos.pop_back();
    }
    
    aguardandoReacao = false;
    
    
    if (cancelado) {
        servidor.broadcast("CANCELADO\n");
        servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");
        notificarTodosMesa(servidor);
        return;
    }
    
    if (nomeAcaoOriginal == "ATACAR") {
        int turnosAtuais = turnosPendentes; 
        turnosPendentes = 0;
        passarTurno(servidor); 
        turnosPendentes = turnosAtuais + 1; 
    } 
    else if (nomeAcaoOriginal == "PULAR") {
        turnosPendentes--;
        if (turnosPendentes <= 0){
            passarTurno(servidor);
        }else {servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");}
    
    }       
    else if (nomeAcaoOriginal == "EMBARALHAR") {
        auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
        std::shuffle(baralho.begin(), baralho.end(), rng);
        servidor.broadcast("EMBARALHOU\n");
        servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");// sempre usar pra carta que não acaba o turno
        
    }
    notificarTodosMesa(servidor);
}

// Ação de comprar carta[cite: 8]
void Partida::comprarCarta(ClienteConectado& cliente, ServidorTCP& servidor) {
    if (baralho.empty()) return;

    auto carta = std::move(baralho.back());
    baralho.pop_back();
    bool sobreviveu = true;

    if (carta->getTipo() == TipoCarta::Bomba) {
        servidor.broadcast("EXPLOSAO " + std::to_string(cliente.jogador.getId()) + "\n");
        
        bool defusou = false;
        int idxDefuse = -1;
        const auto& mao = cliente.jogador.getMao();
        for (size_t i = 0; i < mao.size(); ++i) {
            if (mao[i]->getTipo() == TipoCarta::Desarme) {
                idxDefuse = static_cast<int>(i);
                break;
            }
        }

        if (idxDefuse != -1) {
            auto defuseUsado = cliente.jogador.removerCartaMao(idxDefuse);
            pilhaDescarte.push_back(std::move(defuseUsado));
            defusou = true;
        }

        if (defusou) {
            auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> dist(0, baralho.size());
            baralho.insert(baralho.begin() + dist(rng), std::move(carta));
            servidor.broadcast("DEFUSOU " + std::to_string(cliente.jogador.getId()) + "\n");
        } else {
            removerJogador(cliente.jogador.getId(), false, servidor);
            sobreviveu = false;
        }
    } else {
        std::string nomeCarta = (*carta).getNome();
        cliente.jogador.adicionarCartaMao(std::move(carta));
        
        // CORREÇÃO: Notifica a todos QUE o jogador comprou, e avisa em privado a carta comprada
        servidor.enviarTudo(cliente.socket, "COMPROU_VOCE " + nomeCarta + "\n");
        servidor.broadcast("COMPROU " + std::to_string(cliente.jogador.getId()) + "\n");
    }

    if (emAndamento && sobreviveu) {
        passarTurno(servidor);
    }
    
    // Atualiza a mesa de todos os jogadores vivos
    notificarTodosMesa(servidor);
}

std::shared_ptr<ClienteConectado> Partida::obterJogadorPorId(int id) {
    for (auto& c : jogadores) {
        if (c->jogador.getId() == id) return c;
    }
    return nullptr; 
}

void Partida::removerJogador(int idCliente, bool desconexao, ServidorTCP& servidor) {
    auto it = std::find(ordemTurnos.begin(), ordemTurnos.end(), idCliente);

    if (it != ordemTurnos.end()) {
        ordemTurnos.erase(it);
        numeroJogadoresVivos--;

        // TRATAMENTO DA FILA SEQUENCIAL DO "NÃO"
        if (aguardandoReacao) {
            bool eraOPrimeiro = (!filaRespostaNao.empty() && filaRespostaNao.front() == idCliente);

            auto fit = std::find(filaRespostaNao.begin(), filaRespostaNao.end(), idCliente);
            if (fit != filaRespostaNao.end()) {
                filaRespostaNao.erase(fit);
            }

            if (filaRespostaNao.empty()) {
                aplicarEfeitosPendentes(servidor);
            } 
            else if (eraOPrimeiro) {
                std::string nomeCarta = pilhaEfeitos.empty() ? "CARTA" : pilhaEfeitos.back()->getNome();
                
                // Usa a variável idAutorUltimaCarta em vez de método da pilha
                servidor.broadcast("PERGUNTA_NAO " + std::to_string(idAutorUltimaCarta) + " " + nomeCarta + "\n");
            }
        }

        auto alvo = obterJogadorPorId(idCliente);
        
        if (alvo != nullptr) {
            if (desconexao) {
                while (alvo->jogador.getTamanhoMao() > 0) {
                    auto carta = alvo->jogador.removerCartaMao(0);
                    if (carta->getNome() != "DEFUSE") {
                        baralho.push_back(std::move(carta));
                    }
                }
                for (auto bit = baralho.begin(); bit != baralho.end(); ++bit) {
                    if ((*bit)->getNome() == "BOMBA") {
                        baralho.erase(bit); 
                        break;
                    }
                }
                auto rng = std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count());
                std::shuffle(baralho.begin(), baralho.end(), rng);
            } else {
                while (alvo->jogador.getTamanhoMao() > 0) {
                    pilhaDescarte.push_back(alvo->jogador.removerCartaMao(0));
                }
            }
        }
        
        servidor.broadcast("MORREU " + std::to_string(idCliente) + "\n");
        verificarFimDeJogo(servidor);
    }
}

void Partida::verificarFimDeJogo(ServidorTCP& servidor) {
    if (numeroJogadoresVivos <= 1) {
        emAndamento = false;
        if (!ordemTurnos.empty()) {
            //int vencedorId = jogadores[ordemTurnos.front()]->jogador.getId();
            int vencedorId = ordemTurnos.front();//analisar aqui
            servidor.broadcast("FIM_DE_JOGO VENCEDOR " + std::to_string(vencedorId) + "\n");
        }
    }
}

std::string Partida::obterEstadoMesa(const ClienteConectado& cliente) const {
    std::ostringstream oss;
    oss << "MESA_ESTADO ";
    
    // 1. Baralho e Pilha de Descarte
    oss << "Baralho: " << baralho.size() << " cartas | ";
    oss << "Pilha de Descarte: " << pilhaDescarte.size() << " cartas | ";
    
    // 2. Outros Jogadores
    oss << "Oponentes: ";
    for (const auto& c : jogadores) {
        if (c->jogador.getId() != cliente.jogador.getId() && c->jogador.estaVivo()) {
            oss << c->jogador.getNome() << " (" << c->jogador.getMao().size() << " cartas)   ";
        }
    }
    oss << " | ";

    // 3. Sua Mão
    oss << "Sua mao: ";
    const auto& mao = cliente.jogador.getMao();
    if (mao.empty()) oss << "VAZIA";
    for (size_t i = 0; i < mao.size(); ++i) {
        oss << "[" << i << "] " << mao[i]->getNome() << "   ";
    }
    oss << " | ";

    // 4. Cartas no Descarte (Histórico opcional)
    oss << "Historico_Descarte: ";
    if (pilhaDescarte.empty()) oss << "Vazio";
    for (const auto& c : pilhaDescarte) {
        oss << c->getNome() << " ";
    }
    oss << "\n";
    
    return oss.str();
}

Partida::~Partida() = default;

void Partida::notificarTodosMesa(ServidorTCP& servidor) {
    for (auto& c : jogadores) {
        if (c && c->socket >= 0) {
            servidor.enviarTudo(c->socket, obterEstadoMesa(*c));
        }
    }
}