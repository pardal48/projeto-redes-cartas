#include "Partida.hpp"
#include "Carta.hpp"
#include "Jogador.hpp"
#include "ServidorTCP.hpp"
#include "Conexao.hpp"
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>

namespace {

using std::to_string;

std::vector<std::string> dividirEspacos(const std::string& s) {
    std::istringstream is(s);
    std::vector<std::string> t;
    std::string w;
    while (is >> w) t.push_back(w);
    return t;
}

// Inteiro NÃO-negativo com até 9 dígitos; qualquer outra coisa é rejeitada (sem exceções).
bool lerInteiro(const std::string& s, int& saida) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    saida = std::stoi(s);
    return true;
}

// "3" ou "3,7" ou "3,7,9": de 1 a 3 ids distintos.
bool lerListaIds(const std::string& s, std::vector<int>& ids) {
    ids.clear();
    if (s.empty() || s.back() == ',') return false;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        int v;
        if (!lerInteiro(item, v)) return false;
        if (std::find(ids.begin(), ids.end(), v) != ids.end()) return false;
        ids.push_back(v);
        if (ids.size() > 3) return false;
    }
    return !ids.empty();
}

// Posição na mão da carta com esse id (ou -1).
int acharCarta(Jogador& j, int idCarta) {
    auto& mao = j.getMao();
    for (size_t i = 0; i < mao.size(); ++i)
        if (mao[i]->getId() == idCarta) return static_cast<int>(i);
    return -1;
}

int acharTipo(Jogador& j, TipoCarta tipo) {
    auto& mao = j.getMao();
    for (size_t i = 0; i < mao.size(); ++i)
        if (mao[i]->getTipo() == tipo) return static_cast<int>(i);
    return -1;
}

const auto kJanelaNao = std::chrono::milliseconds(EK_JANELA_NAO_MS);
const auto kResposta = std::chrono::milliseconds(EK_RESPOSTA_MS);

}  // namespace


// Construtor
Partida::Partida(std::vector<std::shared_ptr<ClienteConectado>>& listaClientes) 
    : jogadores(listaClientes), rng(std::random_device{}()) {}

Partida::~Partida() = default;

// ============================ utilidades ============================

std::unique_ptr<Carta> Partida::novaCarta(TipoCarta tipo) {
    // id único por carta física; o nome interno é o token do protocolo ("ATACAR", "GATO1", "DEFUSE"...)
    return Carta::criar(proximoIdCarta++, tipoParaToken(tipo), tipoParaDescricao(tipo), tipo);
}

std::shared_ptr<ClienteConectado> Partida::buscarPorId(int id) const {
    for (const auto& c : jogadores)
        if (c->jogador.getId() == id) return c;
    return nullptr;
}

std::shared_ptr<ClienteConectado> Partida::clienteDaVez() const {
    return ordemTurnos.empty() ? nullptr : jogadores[static_cast<size_t>(ordemTurnos.front())];
}

int Partida::idDaVez() const {
    auto c = clienteDaVez();
    return c ? c->jogador.getId() : 0;
}

const char* Partida::faseToken() const {
    if (!emAndamento) return "FIM";
    switch (fase) {
        case Fase::TurnoNormal:      return "TURNO";
        case Fase::JanelaNao:        return "JANELA";
        case Fase::EscolhendoFavor:  return "FAVOR";
        case Fase::ReinserindoBomba: return "BOMBA";
    }
    return "TURNO";
}

// Cliente já removido pelo servidor tem socket = -1: não se envia nada a ele.
void Partida::enviar(ClienteConectado& c, const std::string& msg) {
    if (srv && c.socket >= 0) srv->enviarTudo(c.socket, msg + "\n");
}

void Partida::transmitir(const std::string& msg) {
    if (srv) srv->broadcast(msg + "\n");  // só chega a quem ainda está na lista de clientes
}

void Partida::enviarMao(ClienteConectado& c) {
    std::string m = "MAO";
    for (auto& carta : c.jogador.getMao())
        m += " " + to_string(carta->getId()) + ":" + tipoParaToken(carta->getTipo());
    enviar(c, m);
}

void Partida::anunciarTurno() {
    transmitir("TURNO " + to_string(idDaVez()) + " " + to_string(turnosPendentes));
}

void Partida::descartar(std::vector<std::unique_ptr<Carta>>& cartas) {
    for (auto& c : cartas) pilhaDescarte.push_back(std::move(c));
    cartas.clear();
}

// ============================ preparação ============================

// Inicialização e Setup do Jogo
void Partida::iniciar(ServidorTCP& servidor) {
    srv = &servidor;
    
    if (jogadores.size() < 2 || jogadores.size() > 5) {
        servidor.broadcast("ERRO NUMERO_JOGADORES_INVALIDO\n");
        return;
    }

    emAndamento = true;
    terminada = false;
    numeroJogadoresVivos = static_cast<int>(jogadores.size());
    fase = Fase::TurnoNormal;
    
    // Configura a ordem dos turnos
    ordemTurnos.clear();
    for (size_t i = 0; i < jogadores.size(); ++i) {
        ordemTurnos.push_back(static_cast<int>(i)); 
    }

    // 1. Cria o baralho base (46 cartas, sem defuses e sem kittens)
    auto adicionar = [this](TipoCarta t, int qtd) {
        for (int i = 0; i < qtd; ++i) baralho.push_back(novaCarta(t));
    };
    adicionar(TipoCarta::Ataque, 4);
    adicionar(TipoCarta::Pular, 4);
    adicionar(TipoCarta::Favor, 4);
    adicionar(TipoCarta::Embaralhar, 4);
    adicionar(TipoCarta::Futuro, 5);
    adicionar(TipoCarta::Nao, 5);
    adicionar(TipoCarta::GatoAranha, 4);
    adicionar(TipoCarta::GatoBarba, 4);
    adicionar(TipoCarta::GatoBatata, 4);
    adicionar(TipoCarta::GatoMelancia, 4);
    adicionar(TipoCarta::GatoTaco, 4);

    // Embaralha o baralho base
    std::shuffle(baralho.begin(), baralho.end(), rng);

    // 2. Distribui as cartas iniciais (1 Defuse + 7 aleatórias)
    for (auto& c : jogadores) {
        c->jogador.adicionarCartaMao(novaCarta(TipoCarta::Desarme));
        for (int i = 0; i < 7; ++i) {
            c->jogador.adicionarCartaMao(std::move(baralho.back()));
            baralho.pop_back();
        }
    }

    // 3. Adiciona Exploding Kittens (jogadores - 1) e Defuses extras (até 2; dos 6 do jogo)
    const int n = static_cast<int>(jogadores.size());
    adicionar(TipoCarta::Bomba, n - 1);
    adicionar(TipoCarta::Desarme, std::min(2, 6 - n));

    // Embaralha o baralho final
    std::shuffle(baralho.begin(), baralho.end(), rng);

    servidor.broadcast("JOGO_INICIADO\n");
    std::string ordem = "ORDEM";
    for (int idx : ordemTurnos) ordem += " " + to_string(jogadores[static_cast<size_t>(idx)]->jogador.getId());
    transmitir(ordem);
    for (auto& c : jogadores) {
        enviarMao(*c);
        servidor.enviarTudo(c->socket, obterEstadoMesa(*c));
    }
    anunciarTurno();
}

// ============================ comandos ============================

// Roteador de Comandos
void Partida::processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor) {
    srv = &servidor;
    std::vector<std::string> t = dividirEspacos(comando);
    if (t.empty()) { enviar(cliente, "ERRO COMANDO_INVALIDO"); return; }
    const std::string& acao = t[0];

    // Consultas valem para qualquer um (vivo, morto, de turno ou não)
    if (acao == "MESA") {
        std::string status = obterEstadoMesa(cliente);
        servidor.enviarTudo(cliente.socket, status);
        return;
    }
    if (acao == "MAO") { enviarMao(cliente); return; }

    // Comandos de turno exigem validação de fase e de turno
    if (!emAndamento) { enviar(cliente, "ERRO JOGO_TERMINADO"); return; }
    if (!cliente.jogador.estaVivo()) { enviar(cliente, "ERRO VOCE_ESTA_MORTO"); return; }

    // se outro jogador tiver jogado uma carta no turno dele: só o "Não" é aceito durante a janela
    if (fase == Fase::JanelaNao) {
        if (acao == "JOGAR_NAO") cmdJogarNao(cliente, t);
        else enviar(cliente, "ERRO AGUARDANDO_RESPOSTA_NAO");
        return;
    }
    if (acao == "JOGAR_NAO") { enviar(cliente, "ERRO NAO_FORA_DA_JANELA"); return; }

    // respostas a decisões pendentes (cada uma só vale na sua fase e para o jogador certo)
    if (acao == "DAR") { cmdDar(cliente, t); return; }
    if (acao == "POSICAO") { cmdPosicao(cliente, t); return; }

    // Ações do turno normal
    if (fase != Fase::TurnoNormal) { enviar(cliente, "ERRO FASE_INVALIDA"); return; }
    if (cliente.jogador.getId() != idDaVez()) { enviar(cliente, "ERRO NAO_E_SEU_TURNO"); return; }

    if (acao == "COMPRAR") {
        cmdComprar(cliente, t);
    } else if (acao == "JOGAR") {
        cmdJogar(cliente, t);
    } else {
        enviar(cliente, "ERRO COMANDO_INVALIDO");
    }
}

// JOGAR <id[,id[,id]]> [<alvo>] [<TIPO_PEDIDO>]
//   1 carta : efeito da carta (Favor exige <alvo>)
//   2 gatos iguais : combo, <alvo> = quem será roubado (carta aleatória)
//   3 gatos iguais : combo, <alvo> e <TIPO_PEDIDO> = carta que se quer
void Partida::cmdJogar(ClienteConectado& cliente, const std::vector<std::string>& t) {
    Jogador& j = cliente.jogador;
    if (t.size() < 2 || t.size() > 4) { enviar(cliente, "ERRO ARGUMENTOS"); return; }

    std::vector<int> ids;
    if (!lerListaIds(t[1], ids)) { enviar(cliente, "ERRO INDICE_INVALIDO"); return; }
    std::vector<int> pos;
    for (int id : ids) {
        int p = acharCarta(j, id);
        if (p < 0) { enviar(cliente, "ERRO INDICE_INVALIDO"); return; }  // carta que não está na mão
        pos.push_back(p);
    }
    auto& mao = j.getMao();
    const TipoCarta tipo = mao[static_cast<size_t>(pos[0])]->getTipo();
    for (int p : pos)
        if (mao[static_cast<size_t>(p)]->getTipo() != tipo) { enviar(cliente, "ERRO COMBO_INVALIDO"); return; }
    if (tipo == TipoCarta::Nao) { enviar(cliente, "ERRO NAO_FORA_DA_JANELA"); return; }

    const Carta& modelo = *mao[static_cast<size_t>(pos[0])];
    bool precisaAlvo = false;
    bool precisaPedido = false;
    if (pos.size() == 1) {
        if (!modelo.jogavelSozinha()) {  // Desarme, Bomba, gato sozinho...
            enviar(cliente, ehGato(tipo) ? "ERRO COMBO_INVALIDO" : "ERRO CARTA_NAO_JOGAVEL");
            return;
        }
        precisaAlvo = modelo.precisaAlvo();
    } else {
        if (!ehGato(tipo)) { enviar(cliente, "ERRO COMBO_INVALIDO"); return; }
        precisaAlvo = true;
        precisaPedido = (pos.size() == 3);
    }

    const size_t esperados = 2 + (precisaAlvo ? 1 : 0) + (precisaPedido ? 1 : 0);
    if (t.size() != esperados) { enviar(cliente, "ERRO ARGUMENTOS"); return; }

    std::shared_ptr<ClienteConectado> alvo;
    int idAlvo = 0;
    if (precisaAlvo) {
        if (!lerInteiro(t[2], idAlvo) || idAlvo == j.getId() || !(alvo = buscarPorId(idAlvo)) ||
            !alvo->jogador.estaVivo()) {
            enviar(cliente, "ERRO ALVO_INVALIDO");
            return;
        }
        // Favor e combo de 2 precisam que o alvo tenha o que dar; o combo de 3 pode falhar (regra do jogo)
        if (pos.size() != 3 && alvo->jogador.getTamanhoMao() == 0) { enviar(cliente, "ERRO ALVO_SEM_CARTAS"); return; }
    }
    TipoCarta pedido = TipoCarta::Pular;
    if (precisaPedido && (!tokenParaTipo(t[3], pedido) || pedido == TipoCarta::Bomba)) {
        enviar(cliente, "ERRO CARTA_PEDIDA_INVALIDA");
        return;
    }

    // Tudo validado: a(s) carta(s) saem da mão e vão para a pilha de efeitos; abre-se a janela do "Não".
    std::string tipos;
    pilhaEfeitos.clear();
    for (int id : ids) {
        int p = acharCarta(j, id);
        if (!tipos.empty()) tipos += ',';
        tipos += tipoParaToken(mao[static_cast<size_t>(p)]->getTipo());
        pilhaEfeitos.push_back(j.removerCartaMao(static_cast<size_t>(p)));
    }
    qtdCartasAcao = ids.size();
    acaoJogadorId = j.getId();
    acaoAlvo = alvo;
    acaoPedido = pedido;
    fase = Fase::JanelaNao;
    prazo = Relogio::now() + kJanelaNao;

    transmitir("JOGOU " + to_string(j.getId()) + " " + tipos + " " + to_string(idAlvo) + " " +
               (precisaPedido ? tipoParaToken(pedido) : "-"));
    transmitir("PERGUNTA_NAO " + to_string(j.getId()) + " " + tipoParaToken(tipo) + " " + to_string(EK_JANELA_NAO_MS / 1000));
    enviarMao(cliente);
}

// "Não" sobre a pilha de efeitos. Cada Não reabre a janela, para que outros possam anulá-lo.
void Partida::cmdJogarNao(ClienteConectado& cliente, const std::vector<std::string>& t) {
    if (t.size() != 1) { enviar(cliente, "ERRO ARGUMENTOS"); return; }
    Jogador& j = cliente.jogador;
    int p = acharTipo(j, TipoCarta::Nao);
    if (p < 0) { enviar(cliente, "ERRO VOCE_NAO_TEM_A_CARTA_NAO"); return; }
    pilhaEfeitos.push_back(j.removerCartaMao(static_cast<size_t>(p)));
    prazo = Relogio::now() + kJanelaNao;
    transmitir("JOGOU_NAO " + to_string(j.getId()));
    transmitir("PERGUNTA_NAO " + to_string(j.getId()) + " NAO " + to_string(EK_JANELA_NAO_MS / 1000));
    enviarMao(cliente);
}

// Fim da janela: a ação vale se o número de "Não" for par (0, 2...), é anulada se for ímpar.
void Partida::resolverJanela() {
    fase = Fase::TurnoNormal;  // antes de aplicar: o efeito pode mudar a fase (Favor) ou o turno
    std::vector<std::unique_ptr<Carta>> pilha = std::move(pilhaEfeitos);
    pilhaEfeitos.clear();
    const size_t qtd = qtdCartasAcao;
    qtdCartasAcao = 0;
    std::shared_ptr<ClienteConectado> alvo = acaoAlvo;
    acaoAlvo.reset();
    const int idJogador = acaoJogadorId;
    const TipoCarta pedido = acaoPedido;
    if (pilha.size() < qtd || qtd == 0) { descartar(pilha); return; }

    const bool cancelado = ((pilha.size() - qtd) % 2) == 1;
    if (cancelado) {
        transmitir("CANCELADO");
        descartar(pilha);
        return;
    }
    transmitir("APLICADO");
    if (qtd == 1) pilha[0]->aplicarEfeito(*srv, alvo);       // polimorfismo: cada carta sabe o que fazer
    else if (qtd == 2) roubarAleatoria(idJogador, alvo);     // combo de 2 gatos
    else if (qtd == 3) roubarEspecifica(idJogador, alvo, pedido);  // combo de 3 gatos
    descartar(pilha);  // as cartas jogadas (e os Não) vão para o descarte
}

// Ação de comprar carta
void Partida::cmdComprar(ClienteConectado& cliente, const std::vector<std::string>& t) {
    if (t.size() != 1) { enviar(cliente, "ERRO ARGUMENTOS"); return; }
    comprarCarta(cliente);
}

void Partida::comprarCarta(ClienteConectado& cliente) {
    if (baralho.empty()) {  // não deveria ocorrer (sempre há uma bomba no baralho); defensivo
        transmitir("AVISO Baralho vazio: turno encerrado sem comprar.");
        encerrarUmTurno();
        return;
    }

    auto carta = std::move(baralho.back());
    baralho.pop_back();
    Jogador& j = cliente.jogador;

    if (carta->getTipo() == TipoCarta::Bomba) {
        transmitir("EXPLOSAO " + to_string(j.getId()));
        int idxDefuse = acharTipo(j, TipoCarta::Desarme);
        if (idxDefuse >= 0) {
            // Usa o Defuse (vai ao descarte) e o jogador escolhe onde esconder a bomba (comando POSICAO).
            pilhaDescarte.push_back(j.removerCartaMao(static_cast<size_t>(idxDefuse)));
            bombaEmMao = std::move(carta);
            bombaJogadorId = j.getId();
            fase = Fase::ReinserindoBomba;
            prazo = Relogio::now() + kResposta;
            transmitir("DEFUSOU " + to_string(j.getId()));
            enviar(cliente, "REINSERIR " + to_string(baralho.size()) + " " + to_string(EK_RESPOSTA_MS / 1000));
            enviarMao(cliente);
        } else {
            pilhaDescarte.push_back(std::move(carta));         // a bomba fica à vista no descarte
            removerJogador(j.getId(), false, *srv);            // explode
        }
        return;
    }

    // CORREÇÃO: o token é lido ANTES do std::move (depois do move 'carta' é nullptr)
    const std::string token = tipoParaToken(carta->getTipo());
    j.adicionarCartaMao(std::move(carta));
    enviar(cliente, "COMPROU " + token);
    enviarMao(cliente);
    transmitir("COMPRA " + to_string(j.getId()) + " " + to_string(baralho.size()));
    encerrarUmTurno();
}

// POSICAO <n>: n cartas ficam acima da bomba (0 = topo ... tamanho do baralho = fundo)
void Partida::cmdPosicao(ClienteConectado& cliente, const std::vector<std::string>& t) {
    if (fase != Fase::ReinserindoBomba) { enviar(cliente, "ERRO FASE_INVALIDA"); return; }
    if (cliente.jogador.getId() != bombaJogadorId) { enviar(cliente, "ERRO NAO_E_SUA_ESCOLHA"); return; }
    int n;
    if (t.size() != 2 || !lerInteiro(t[1], n) || static_cast<size_t>(n) > baralho.size()) {
        enviar(cliente, "ERRO POSICAO_INVALIDA");
        return;
    }
    reinserirBomba(static_cast<size_t>(n));
}

void Partida::reinserirBomba(size_t posicaoDoTopo) {
    if (descartarBombaReinserida) {
        // Um jogador saiu enquanto esta bomba estava na mão: ela sai do jogo (bombas = vivos - 1).
        descartarBombaReinserida = false;
        bombaEmMao.reset();
        transmitir("AVISO A bomba foi removida do jogo para manter o equilibrio.");
    } else if (bombaEmMao) {
        posicaoDoTopo = std::min(posicaoDoTopo, baralho.size());
        // topo = back(): 'posicaoDoTopo' cartas acima => índice = tamanho - posicao
        baralho.insert(baralho.begin() + static_cast<std::ptrdiff_t>(baralho.size() - posicaoDoTopo), std::move(bombaEmMao));
    }
    fase = Fase::TurnoNormal;
    transmitir("BOMBA_REINSERIDA " + to_string(bombaJogadorId));
    encerrarUmTurno();  // comprar a bomba já contou como a compra deste turno
}

// DAR <id_da_carta>: o alvo do Favor escolhe a carta que entrega
void Partida::cmdDar(ClienteConectado& cliente, const std::vector<std::string>& t) {
    if (fase != Fase::EscolhendoFavor) { enviar(cliente, "ERRO FASE_INVALIDA"); return; }
    if (cliente.jogador.getId() != favorAlvoId) { enviar(cliente, "ERRO NAO_E_SUA_ESCOLHA"); return; }
    int idCarta;
    int p;
    if (t.size() != 2 || !lerInteiro(t[1], idCarta) || (p = acharCarta(cliente.jogador, idCarta)) < 0) {
        enviar(cliente, "ERRO INDICE_INVALIDO");
        return;
    }
    concluirFavor(static_cast<size_t>(p));
}

void Partida::concluirFavor(size_t indiceNaMaoDoAlvo) {
    auto alvo = buscarPorId(favorAlvoId);
    auto solicitante = buscarPorId(favorSolicitanteId);
    fase = Fase::TurnoNormal;
    if (alvo && solicitante) transferir(*alvo, *solicitante, indiceNaMaoDoAlvo, "FAVOR");
    transmitir("FAVOR_CONCLUIDO " + to_string(favorSolicitanteId) + " " + to_string(favorAlvoId));
}

// ============================ transferências e efeitos ============================

bool Partida::transferir(ClienteConectado& de, ClienteConectado& para, size_t indice, const char* origem) {
    auto carta = de.jogador.removerCartaMao(indice);
    if (!carta) return false;
    const std::string token = tipoParaToken(carta->getTipo());  // antes do move
    para.jogador.adicionarCartaMao(std::move(carta));
    enviar(para, std::string("RECEBEU ") + token + " " + origem);
    enviar(de, "PERDEU " + token);
    enviarMao(para);
    enviarMao(de);
    return true;
}

void Partida::roubarAleatoria(int idLadrao, std::shared_ptr<ClienteConectado> vitima) {
    auto ladrao = buscarPorId(idLadrao);
    if (!ladrao || !vitima || vitima->jogador.getTamanhoMao() == 0) {
        transmitir("ROUBO " + to_string(idLadrao) + " " + to_string(vitima ? vitima->jogador.getId() : 0) + " 0");
        return;
    }
    std::uniform_int_distribution<size_t> dist(0, vitima->jogador.getTamanhoMao() - 1);
    const bool ok = transferir(*vitima, *ladrao, dist(rng), "ROUBO");
    transmitir("ROUBO " + to_string(idLadrao) + " " + to_string(vitima->jogador.getId()) + (ok ? " 1" : " 0"));
}

// Combo de 3: se o alvo não tiver a carta pedida, o jogador não recebe nada.
void Partida::roubarEspecifica(int idLadrao, std::shared_ptr<ClienteConectado> vitima, TipoCarta pedido) {
    auto ladrao = buscarPorId(idLadrao);
    bool ok = false;
    if (ladrao && vitima) {
        int p = acharTipo(vitima->jogador, pedido);
        if (p >= 0) ok = transferir(*vitima, *ladrao, static_cast<size_t>(p), "ROUBO");
    }
    transmitir("ROUBO " + to_string(idLadrao) + " " + to_string(vitima ? vitima->jogador.getId() : 0) + (ok ? " 1" : " 0"));
}

void Partida::encerrarTurnoSemComprar() { encerrarUmTurno(); }

void Partida::atacarProximoJogador() {
    // Atacar encerra TODO o turno do atacante. Se ele próprio estava sob ataque, as ordens se
    // acumulam: (turnos que ainda devia) + 2. Caso contrário, o próximo faz 2 turnos.
    passarTurno(porAtaque ? turnosPendentes + 2 : 2, true);
}

void Partida::embaralharBaralho() {
    std::shuffle(baralho.begin(), baralho.end(), rng);
    transmitir("EMBARALHOU " + to_string(idDaVez()));
}

void Partida::mostrarFuturo() {
    auto vez = clienteDaVez();
    if (!vez) return;
    std::string m = "FUTURO";
    for (size_t i = 0; i < 3 && i < baralho.size(); ++i)
        m += std::string(" ") + tipoParaToken(baralho[baralho.size() - 1 - i]->getTipo());
    enviar(*vez, m);
}

void Partida::iniciarFavor(std::shared_ptr<ClienteConectado> alvo) {
    auto vez = clienteDaVez();
    if (!vez) return;
    if (!alvo || !alvo->jogador.estaVivo() || alvo->jogador.getTamanhoMao() == 0) {
        // O alvo ficou sem cartas durante a janela (ex.: gastou Nãos): nada a entregar.
        transmitir("FAVOR_SEM_CARTAS " + to_string(vez->jogador.getId()) + " " + to_string(alvo ? alvo->jogador.getId() : 0));
        return;
    }
    fase = Fase::EscolhendoFavor;
    favorSolicitanteId = vez->jogador.getId();
    favorAlvoId = alvo->jogador.getId();
    prazo = Relogio::now() + kResposta;
    transmitir("FAVOR_PEDIDO " + to_string(favorSolicitanteId) + " " + to_string(favorAlvoId));
    enviar(*alvo, "DAR_CARTA " + to_string(favorSolicitanteId) + " " + to_string(EK_RESPOSTA_MS / 1000));
    enviarMao(*alvo);
}

// ============================ turnos ============================

void Partida::encerrarUmTurno() {
    --turnosPendentes;
    if (turnosPendentes <= 0) passarTurno(1, false);
    else anunciarTurno();  // ainda restam turnos (efeito de um Atacar)
}

// Gira a lista circular: o jogador da vez vai para o fim da fila; quem era o próximo passa a ser a frente.
void Partida::passarTurno(int turnos, bool atacado) {
    if (ordemTurnos.empty()) return;
    int atual = ordemTurnos.front();
    ordemTurnos.pop_front();
    ordemTurnos.push_back(atual);
    turnosPendentes = turnos;
    porAtaque = atacado;
    anunciarTurno();
}

// Morte (desconexao=false) ou saída/queda (desconexao=true) de um jogador.
void Partida::removerJogador(int idCliente, bool desconexao, ServidorTCP& servidor) {
    srv = &servidor;
    if (!emAndamento) return;
    auto cj = buscarPorId(idCliente);
    if (!cj) return;

    auto it = std::find_if(ordemTurnos.begin(), ordemTurnos.end(), [this, idCliente](int idx) {
        return jogadores[static_cast<size_t>(idx)]->jogador.getId() == idCliente;
    });

    if (it == ordemTurnos.end()) {
        // Jogador morto desconectando: apenas emite aviso
        if (desconexao) transmitir("SAIU " + to_string(idCliente) + " MORTO");
        return;
    }

    const bool eraDaVez = (it == ordemTurnos.begin());
    Jogador& jd = cj->jogador;

    if (desconexao) {
        // Regra: se desconecta vivo, as cartas voltam ao baralho (menos o Defuse, que sai do jogo),
        // o baralho é embaralhado e uma bomba é removida para manter bombas = vivos - 1.
        transmitir("SAIU " + to_string(idCliente) + " VIVO");
        const bool segurandoBomba = (fase == Fase::ReinserindoBomba && bombaJogadorId == idCliente);
        while (jd.getTamanhoMao() > 0) {
            auto carta = jd.removerCartaMao(0);
            if (carta->getTipo() != TipoCarta::Desarme) baralho.push_back(std::move(carta));
        }
        if (segurandoBomba) {
            bombaEmMao.reset();  // a bomba que ele acabou de comprar sai do jogo no lugar de outra
        } else if (!removerBombaAleatoria() && fase == Fase::ReinserindoBomba) {
            // A única bomba restante está na mão de outro jogador que ainda vai reinseri-la.
            descartarBombaReinserida = true;
        }
        std::shuffle(baralho.begin(), baralho.end(), rng);
    } else {
        // Morreu por explosão: move todas as cartas da mão para o descarte
        while (jd.getTamanhoMao() > 0) pilhaDescarte.push_back(jd.removerCartaMao(0));
        transmitir("MORREU " + to_string(idCliente));
    }

    jd.eliminar();
    ordemTurnos.erase(it);
    numeroJogadoresVivos--;

    // ações pendentes que dependiam dele são canceladas
    if (fase == Fase::JanelaNao && (acaoJogadorId == idCliente || (acaoAlvo && acaoAlvo->jogador.getId() == idCliente))) {
        descartar(pilhaEfeitos);
        qtdCartasAcao = 0;
        acaoAlvo.reset();
        fase = Fase::TurnoNormal;
        transmitir("CANCELADO");
    } else if (fase == Fase::EscolhendoFavor && (favorSolicitanteId == idCliente || favorAlvoId == idCliente)) {
        fase = Fase::TurnoNormal;
        transmitir("FAVOR_CANCELADO");
    } else if (fase == Fase::ReinserindoBomba && bombaJogadorId == idCliente) {
        fase = Fase::TurnoNormal;
        descartarBombaReinserida = false;
    }

    verificarFimDeJogo();
    if (emAndamento && eraDaVez) {
        // 'ordemTurnos.front()' já é o próximo jogador (o removido era a frente)
        turnosPendentes = 1;
        porAtaque = false;
        anunciarTurno();
    }
}

bool Partida::removerBombaAleatoria() {
    std::vector<size_t> bombas;
    for (size_t i = 0; i < baralho.size(); ++i)
        if (baralho[i]->getTipo() == TipoCarta::Bomba) bombas.push_back(i);
    if (bombas.empty()) return false;
    std::uniform_int_distribution<size_t> dist(0, bombas.size() - 1);
    baralho.erase(baralho.begin() + static_cast<std::ptrdiff_t>(bombas[dist(rng)]));
    return true;
}

void Partida::verificarFimDeJogo() {
    if (numeroJogadoresVivos <= 1 && emAndamento) {
        emAndamento = false;
        terminada = true;
        fase = Fase::TurnoNormal;
        pilhaEfeitos.clear();
        qtdCartasAcao = 0;
        if (!ordemTurnos.empty()) {
            int vencedorId = jogadores[static_cast<size_t>(ordemTurnos.front())]->jogador.getId();
            transmitir("FIM_DE_JOGO VENCEDOR " + to_string(vencedorId));
        } else {
            transmitir("FIM_DE_JOGO EMPATE");
        }
    }
}

// ============================ tempo ============================

std::optional<Partida::Instante> Partida::proximoPrazo() const {
    if (emAndamento && fase != Fase::TurnoNormal) return prazo;
    return std::nullopt;
}

void Partida::processarTempo(ServidorTCP& servidor, Instante agora) {
    srv = &servidor;
    std::optional<Instante> p = proximoPrazo();
    if (!p || agora < *p) return;

    switch (fase) {
        case Fase::JanelaNao:
            resolverJanela();
            break;
        case Fase::EscolhendoFavor: {
            // O alvo não respondeu a tempo: o servidor escolhe uma carta aleatória por ele.
            auto alvo = buscarPorId(favorAlvoId);
            if (alvo && alvo->jogador.getTamanhoMao() > 0) {
                std::uniform_int_distribution<size_t> dist(0, alvo->jogador.getTamanhoMao() - 1);
                transmitir("AVISO Tempo esgotado: carta do favor escolhida automaticamente.");
                concluirFavor(dist(rng));
            } else {
                fase = Fase::TurnoNormal;
                transmitir("FAVOR_SEM_CARTAS " + to_string(favorSolicitanteId) + " " + to_string(favorAlvoId));
            }
            break;
        }
        case Fase::ReinserindoBomba: {
            std::uniform_int_distribution<size_t> dist(0, baralho.size());
            transmitir("AVISO Tempo esgotado: bomba reinserida em posicao aleatoria.");
            reinserirBomba(dist(rng));
            break;
        }
        case Fase::TurnoNormal:
            break;
    }
}

std::string Partida::obterEstadoMesa(const ClienteConectado& /*cliente*/) const {
    std::ostringstream oss;
    oss << "STATUS VEZ:" << idDaVez()
        << " TURNOS:" << turnosPendentes
        << " JOGADORES_VIVOS:" << numeroJogadoresVivos 
        << " BARALHO:" << baralho.size() 
        << " DESCARTE:" << pilhaDescarte.size()
        << " TOPO:" << (pilhaDescarte.empty() ? "-" : tipoParaToken(pilhaDescarte.back()->getTipo()))
        << " FASE:" << faseToken()
        << " JOGADORES:";
    bool primeiro = true;
    for (const auto& c : jogadores) {
        if (!primeiro) oss << ';';
        primeiro = false;
        oss << c->jogador.getId() << ':' << (c->jogador.estaVivo() ? 1 : 0) << ':' << c->jogador.getTamanhoMao();
    }
    oss << "\n";
    return oss.str();
}
