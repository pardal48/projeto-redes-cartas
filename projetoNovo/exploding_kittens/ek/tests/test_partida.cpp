// Teste da Partida + ServidorTCP SEM TCP real: cada cliente é um socketpair (o servidor escreve numa
// ponta, o teste lê na outra). Parte 1: cenários determinísticos das regras. Parte 2: milhares de
// partidas aleatórias com comandos malformados e desconexões, checando invariantes a cada passo.
// Compilar/rodar: make teste_partida
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

// Só neste teste: acesso aos membros privados para montar cenários e checar invariantes.
#define private public
#include "../ServidorTCP.hpp"
#include "../Partida.hpp"
#undef private

static int falhas = 0, verificacoes = 0;
#define OK(cond, ...)                                                           \
    do {                                                                        \
        ++verificacoes;                                                         \
        if (!(cond)) {                                                          \
            ++falhas;                                                           \
            std::printf("FALHA (linha %d): %s | ", __LINE__, #cond);            \
            std::printf(__VA_ARGS__);                                           \
            std::printf("\n");                                                  \
            if (falhas > 25) std::exit(1);                                      \
        }                                                                       \
    } while (0)

struct Fake {
    std::shared_ptr<ClienteConectado> c;
    int peer = -1;
    std::string buf;
    std::vector<std::string> msgs;
};

struct Mundo {
    std::unique_ptr<ServidorTCP> srv;
    std::vector<Fake> fakes;

    explicit Mundo(int n) {
        srv = std::make_unique<ServidorTCP>(0, "teste");
        for (int i = 1; i <= n; ++i) {
            int sv[2];
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
            auto c = std::make_shared<ClienteConectado>();
            c->socket = sv[0];
            c->jogador.setId(i);
            c->jogador.setNome("J" + std::to_string(i));
            srv->clientes.push_back(c);
            Fake f; f.c = c; f.peer = sv[1];
            fakes.push_back(std::move(f));
        }
        srv->iniciarPartida();  // mesmo caminho do servidor real (broadcast INICIAR + Partida::iniciar)
        drain();
    }
    ~Mundo() { for (auto& f : fakes) if (f.peer >= 0) close(f.peer); }

    Partida& p() { return *srv->partidaAtual; }

    void drain() {
        for (auto& f : fakes) {
            if (f.peer < 0) continue;
            char b[4096];
            for (;;) {
                ssize_t n = recv(f.peer, b, sizeof b, MSG_DONTWAIT);
                if (n <= 0) break;
                f.buf.append(b, static_cast<size_t>(n));
            }
            size_t pos;
            while ((pos = f.buf.find('\n')) != std::string::npos) {
                f.msgs.push_back(f.buf.substr(0, pos));
                f.buf.erase(0, pos + 1);
            }
            if (f.msgs.size() > 400) f.msgs.erase(f.msgs.begin(), f.msgs.begin() + 200);
        }
    }
    void cmd(int i, const std::string& linha) {
        { std::lock_guard<std::mutex> lk(srv->mtx); srv->processarLinha(*fakes[static_cast<size_t>(i - 1)].c, linha); }
        drain();
    }
    void tempo() {
        { std::lock_guard<std::mutex> lk(srv->mtx);
          if (srv->partidaAtual) srv->partidaAtual->processarTempo(*srv, Partida::Instante::max());
          srv->verificarFimDeJogo(); }
        drain();
    }
    void desconectar(int i) {
        Fake& f = fakes[static_cast<size_t>(i - 1)];
        srv->removerCliente(f.c);  // trava o mtx sozinho, fecha o socket, aplica as regras
        close(f.peer); f.peer = -1;
        drain();
    }
    bool pub(const std::string& prefixo) {
        for (auto& f : fakes) for (auto& m : f.msgs) if (m.rfind(prefixo, 0) == 0) return true;
        return false;
    }
    bool priv(int i, const std::string& prefixo) {
        for (auto& m : fakes[static_cast<size_t>(i - 1)].msgs) if (m.rfind(prefixo, 0) == 0) return true;
        return false;
    }
    int dar(int i, TipoCarta t) {
        auto carta = p().novaCarta(t);
        int id = carta->getId();
        fakes[static_cast<size_t>(i - 1)].c->jogador.adicionarCartaMao(std::move(carta));
        return id;
    }
    void limparMaos() { for (auto& f : fakes) while (f.c->jogador.getTamanhoMao()) f.c->jogador.removerCartaMao(0); }
    void topo(TipoCarta t) { p().baralho.push_back(p().novaCarta(t)); }
    size_t mao(int i) { return fakes[static_cast<size_t>(i - 1)].c->jogador.getTamanhoMao(); }
    int vez() { return p().idDaVez(); }
    int pend() { return p().turnosPendentes; }
};

static std::string S(int x) { return std::to_string(x); }

static void cenarios() {
    {   // setup: mãos de 8, bombas e defuses
        for (int n = 2; n <= 5; ++n) {
            Mundo m(n);
            int bombas = 0, defuses = 0;
            for (auto& f : m.fakes) { OK(f.c->jogador.getTamanhoMao() == 8, "mao inicial"); for (auto& k : f.c->jogador.getMao()) defuses += k->getTipo() == TipoCarta::Desarme; }
            for (auto& k : m.p().baralho) { bombas += k->getTipo() == TipoCarta::Bomba; defuses += k->getTipo() == TipoCarta::Desarme; }
            OK(bombas == n - 1, "bombas=%d n=%d", bombas, n);
            OK(defuses == n + std::min(2, 6 - n), "defuses=%d n=%d", defuses, n);
            OK(m.p().baralho.size() == 46 - 7u * n + (n - 1) + std::min(2, 6 - n), "tamanho do baralho");
            OK(m.pub("INICIAR") && m.pub("JOGO_INICIADO") && m.pub("TURNO 1 1"), "mensagens de inicio");
        }
    }
    {   Mundo m(3);  // Atacar simples
        m.limparMaos();
        int a = m.dar(1, TipoCarta::Ataque);
        m.cmd(1, "JOGAR " + S(a));
        OK(m.p().fase == Partida::Fase::JanelaNao && m.pub("PERGUNTA_NAO 1 ATACAR"), "janela abriu");
        m.cmd(1, "COMPRAR");
        OK(m.priv(1, "ERRO AGUARDANDO_RESPOSTA_NAO"), "nao compra na janela");
        m.tempo();
        OK(m.vez() == 2 && m.pend() == 2 && m.pub("APLICADO"), "J2 faz 2 turnos");
    }
    {   Mundo m(3);  // Atacar acumulado: 2 + 2 = 4
        m.limparMaos();
        int a = m.dar(1, TipoCarta::Ataque), b = m.dar(2, TipoCarta::Ataque);
        m.cmd(1, "JOGAR " + S(a)); m.tempo();
        m.cmd(2, "JOGAR " + S(b)); m.tempo();
        OK(m.vez() == 3 && m.pend() == 4, "J3 faz 4 turnos (%d)", m.pend());
    }
    {   Mundo m(3);  // completa 1 turno e ataca no 2º: 1 + 2 = 3
        m.limparMaos();
        int a = m.dar(1, TipoCarta::Ataque), b = m.dar(2, TipoCarta::Ataque);
        m.cmd(1, "JOGAR " + S(a)); m.tempo();
        m.topo(TipoCarta::Pular);
        m.cmd(2, "COMPRAR");
        OK(m.vez() == 2 && m.pend() == 1, "J2 ainda tem 1 turno");
        m.cmd(2, "JOGAR " + S(b)); m.tempo();
        OK(m.vez() == 3 && m.pend() == 3, "J3 faz 3 turnos (%d)", m.pend());
    }
    {   Mundo m(3);  // Pular dentro de ataque consome 1 de 2
        m.limparMaos();
        int a = m.dar(1, TipoCarta::Ataque), p = m.dar(2, TipoCarta::Pular);
        m.cmd(1, "JOGAR " + S(a)); m.tempo();
        m.cmd(2, "JOGAR " + S(p)); m.tempo();
        OK(m.vez() == 2 && m.pend() == 1, "Pular consome 1 de 2 turnos");
    }
    {   Mundo m(3);  // Não: 1 cancela
        m.limparMaos();
        int p = m.dar(1, TipoCarta::Pular); m.dar(2, TipoCarta::Nao); m.dar(3, TipoCarta::Nao);
        m.cmd(1, "JOGAR " + S(p)); m.cmd(2, "JOGAR_NAO"); m.tempo();
        OK(m.pub("CANCELADO") && m.vez() == 1 && !m.pub("APLICADO"), "1 Nao cancela");
        OK(m.p().pilhaDescarte.size() == 2, "carta e Nao vao ao descarte");
    }
    {   Mundo m(3);  // Não sobre Não: efeito vale
        m.limparMaos();
        int p = m.dar(1, TipoCarta::Pular); m.dar(2, TipoCarta::Nao); m.dar(3, TipoCarta::Nao);
        m.cmd(1, "JOGAR " + S(p)); m.cmd(2, "JOGAR_NAO"); m.cmd(3, "JOGAR_NAO"); m.tempo();
        OK(m.pub("APLICADO") && m.vez() == 2, "2 Naos: efeito vale");
        OK(m.p().pilhaDescarte.size() == 3 && m.p().pilhaEfeitos.empty(), "pilha de efeitos esvaziada");
    }
    {   Mundo m(2);  // Não: erros
        m.limparMaos();
        m.cmd(2, "JOGAR_NAO");
        OK(m.priv(2, "ERRO NAO_FORA_DA_JANELA"), "Nao fora da janela");
        int p = m.dar(1, TipoCarta::Pular);
        m.cmd(1, "JOGAR " + S(p)); m.cmd(2, "JOGAR_NAO");
        OK(m.priv(2, "ERRO VOCE_NAO_TEM_A_CARTA_NAO"), "sem Nao na mao");
    }
    {   Mundo m(3);  // combos
        m.limparMaos();
        int g1 = m.dar(1, TipoCarta::GatoAranha), g2 = m.dar(1, TipoCarta::GatoAranha); m.dar(2, TipoCarta::Desarme);
        m.cmd(1, "JOGAR " + S(g1) + "," + S(g2) + " 2"); m.tempo();
        OK(m.mao(1) == 1 && m.mao(2) == 0 && m.pub("ROUBO 1 2 1"), "combo de 2 roubou");

        Mundo d(3); d.limparMaos();
        int a = d.dar(1, TipoCarta::GatoBarba), b = d.dar(1, TipoCarta::GatoBarba), e = d.dar(1, TipoCarta::GatoBarba);
        d.dar(2, TipoCarta::Desarme);
        d.cmd(1, "JOGAR " + S(a) + "," + S(b) + "," + S(e) + " 2 DEFUSE"); d.tempo();
        OK(d.mao(1) == 1 && d.mao(2) == 0, "combo de 3 acertou");

        Mundo f(3); f.limparMaos();
        a = f.dar(1, TipoCarta::GatoBarba); b = f.dar(1, TipoCarta::GatoBarba); e = f.dar(1, TipoCarta::GatoBarba);
        f.dar(2, TipoCarta::Pular);
        f.cmd(1, "JOGAR " + S(a) + "," + S(b) + "," + S(e) + " 2 DEFUSAR"); f.tempo();
        OK(f.mao(1) == 0 && f.mao(2) == 1 && f.pub("ROUBO 1 2 0"), "combo de 3 errou: nada acontece");

        Mundo h(3); h.limparMaos();
        a = h.dar(1, TipoCarta::GatoAranha); b = h.dar(1, TipoCarta::GatoBarba);
        h.cmd(1, "JOGAR " + S(a) + "," + S(b) + " 2");
        OK(h.priv(1, "ERRO COMBO_INVALIDO"), "gatos diferentes");
        h.cmd(1, "JOGAR " + S(a));
        OK(h.priv(1, "ERRO COMBO_INVALIDO"), "gato sozinho");
        int de = h.dar(1, TipoCarta::Desarme);
        h.cmd(1, "JOGAR " + S(de));
        OK(h.priv(1, "ERRO CARTA_NAO_JOGAVEL"), "defuse nao e jogavel");
        h.cmd(1, "JOGAR 99999");
        OK(h.priv(1, "ERRO INDICE_INVALIDO"), "carta fora da mao");
        h.cmd(1, "JOGAR 1,1");
        OK(h.priv(1, "ERRO INDICE_INVALIDO"), "ids repetidos");
    }
    {   Mundo m(3);  // Favor: validações, escolha e timeout
        m.limparMaos();
        int fv = m.dar(1, TipoCarta::Favor); int x = m.dar(2, TipoCarta::Pular); m.dar(2, TipoCarta::Ataque);
        m.cmd(1, "JOGAR " + S(fv));
        OK(m.priv(1, "ERRO ARGUMENTOS"), "favor sem alvo");
        m.cmd(1, "JOGAR " + S(fv) + " 1");
        OK(m.priv(1, "ERRO ALVO_INVALIDO"), "alvo = si mesmo");
        m.cmd(1, "JOGAR " + S(fv) + " 3");
        OK(m.priv(1, "ERRO ALVO_SEM_CARTAS"), "alvo sem cartas");
        m.cmd(1, "JOGAR " + S(fv) + " 2"); m.tempo();
        OK(m.priv(2, "DAR_CARTA 1"), "alvo avisado");
        m.cmd(3, "DAR " + S(x));
        OK(m.priv(3, "ERRO NAO_E_SUA_ESCOLHA"), "so o alvo responde");
        m.cmd(2, "DAR 99999");
        OK(m.priv(2, "ERRO INDICE_INVALIDO"), "carta fora da mao");
        m.cmd(2, "DAR " + S(x));
        OK(m.mao(1) == 1 && m.mao(2) == 1 && m.p().fase == Partida::Fase::TurnoNormal && m.pub("FAVOR_CONCLUIDO 1 2"), "favor entregue");

        Mundo d(3); d.limparMaos();
        fv = d.dar(1, TipoCarta::Favor); d.dar(2, TipoCarta::Pular);
        d.cmd(1, "JOGAR " + S(fv) + " 2"); d.tempo(); d.tempo();
        OK(d.mao(1) == 1 && d.mao(2) == 0 && d.pub("FAVOR_CONCLUIDO"), "timeout sorteou");
    }
    {   Mundo m(3);  // Embaralhar e Futuro
        m.limparMaos();
        int e = m.dar(1, TipoCarta::Embaralhar), f = m.dar(1, TipoCarta::Futuro);
        m.topo(TipoCarta::Pular);
        m.cmd(1, "JOGAR " + S(f)); m.tempo();
        OK(m.priv(1, "FUTURO PULAR"), "futuro mostra o topo");
        OK(!m.priv(2, "FUTURO"), "futuro e privado");
        m.cmd(1, "JOGAR " + S(e)); m.tempo();
        OK(m.pub("EMBARALHOU 1") && m.vez() == 1, "embaralhar nao encerra o turno");
    }
    {   Mundo m(3);  // Bomba sem Defuse
        m.limparMaos();
        m.topo(TipoCarta::Bomba);
        m.cmd(1, "COMPRAR");
        OK(m.pub("EXPLOSAO 1") && m.pub("MORREU 1") && !m.fakes[0].c->jogador.estaVivo(), "explodiu");
        OK(m.vez() == 2 && m.pend() == 1 && m.p().ordemTurnos.size() == 2, "turno passou ao proximo");
        m.cmd(1, "COMPRAR");
        OK(m.priv(1, "ERRO VOCE_ESTA_MORTO"), "morto nao joga");
        m.cmd(1, "MESA");
        OK(m.priv(1, "STATUS"), "morto pode consultar a mesa");
    }
    {   Mundo m(3);  // Bomba com Defuse
        m.limparMaos();
        m.dar(1, TipoCarta::Desarme);
        m.topo(TipoCarta::Bomba);
        size_t tam = m.p().baralho.size() - 1;
        m.cmd(1, "COMPRAR");
        OK(m.p().fase == Partida::Fase::ReinserindoBomba && m.priv(1, "REINSERIR " + S(static_cast<int>(tam))) && m.pub("DEFUSOU 1"), "pediu reinsercao");
        m.cmd(2, "POSICAO 0");
        OK(m.priv(2, "ERRO NAO_E_SUA_ESCOLHA"), "so quem comprou");
        m.cmd(1, "POSICAO 99999");
        OK(m.priv(1, "ERRO POSICAO_INVALIDA"), "posicao fora do limite");
        m.cmd(1, "POSICAO 0");
        OK(m.p().baralho.back()->getTipo() == TipoCarta::Bomba && m.vez() == 2, "bomba no topo; turno passou");
        OK(m.p().pilhaDescarte.back()->getTipo() == TipoCarta::Desarme, "defuse no descarte");
    }
    {   Mundo m(2);  // último a explodir: FIM e retorno ao lobby
        m.limparMaos();
        m.fakes[0].c->jogador.setPronto(true); m.fakes[1].c->jogador.setPronto(true);
        m.topo(TipoCarta::Bomba);
        m.cmd(1, "COMPRAR");
        OK(m.pub("FIM_DE_JOGO VENCEDOR 2"), "vencedor J2");
        OK(!m.srv->jogoIniciado && !m.srv->partidaAtual, "servidor voltou ao lobby");
        OK(m.fakes[0].c->jogador.estaVivo() && !m.fakes[0].c->jogador.getPronto() && m.mao(1) == 0, "jogador resetado");
        OK(m.priv(2, "LOBBY "), "lobby reenviado");
    }
    {   Mundo m(3);  // desconexão de vivo: cartas voltam, Defuse some, uma bomba sai
        m.limparMaos();
        m.dar(2, TipoCarta::Desarme); m.dar(2, TipoCarta::Pular);
        auto conta = [&](TipoCarta t) { int n = 0; for (auto& k : m.p().baralho) n += k->getTipo() == t; return n; };
        size_t antes = m.p().baralho.size(); int bombas = conta(TipoCarta::Bomba), defuses = conta(TipoCarta::Desarme), pulares = conta(TipoCarta::Pular);
        m.desconectar(2);
        OK(conta(TipoCarta::Bomba) == bombas - 1, "uma bomba removida");
        OK(conta(TipoCarta::Pular) == pulares + 1 && conta(TipoCarta::Desarme) == defuses, "Pular voltou; Defuse fora do jogo");
        OK(m.p().baralho.size() == antes + 1 - 1, "tamanho do baralho");
        OK(m.p().ordemTurnos.size() == 2 && m.pub("SAIU 2 VIVO"), "saiu da ordem e avisou");
        OK(m.fakes[0].c->jogador.estaVivo() && m.fakes[2].c->jogador.estaVivo(), "demais seguem vivos");
    }
    {   Mundo m(3);  // jogador da vez cai durante a janela: ação cancelada, vez do próximo
        m.limparMaos();
        int p = m.dar(1, TipoCarta::Pular);
        m.cmd(1, "JOGAR " + S(p));
        m.desconectar(1);
        OK(m.p().fase == Partida::Fase::TurnoNormal && m.pub("CANCELADO") && m.vez() == 2 && m.pend() == 1, "acao cancelada");
        m.desconectar(1 + 0 == 1 ? 2 : 2);
        OK(m.pub("FIM_DE_JOGO VENCEDOR 3"), "um jogador restante vence");
    }
    {   Mundo m(3);  // morto que desconecta: só aviso
        m.limparMaos();
        m.topo(TipoCarta::Bomba);
        m.cmd(1, "COMPRAR");
        m.desconectar(1);
        OK(m.pub("SAIU 1 MORTO") && m.p().ordemTurnos.size() == 2, "morto saindo: apenas aviso");
    }
    {   Mundo m(3);  // alvo de combo cai durante a janela: combo cancelado
        m.limparMaos();
        int a = m.dar(1, TipoCarta::GatoTaco), b = m.dar(1, TipoCarta::GatoTaco); m.dar(2, TipoCarta::Pular);
        m.cmd(1, "JOGAR " + S(a) + "," + S(b) + " 2");
        m.desconectar(2);
        OK(m.pub("CANCELADO") && m.p().fase == Partida::Fase::TurnoNormal && m.vez() == 1, "alvo caiu: cancelado, segue a vez de J1");
    }
    {   Mundo m(3);  // quem reinsere a bomba cai: a bomba sai do jogo
        m.limparMaos();
        m.dar(1, TipoCarta::Desarme);
        m.p().removerBombaAleatoria();  // mantém bombas = vivos - 1 depois de injetar a do topo
        m.topo(TipoCarta::Bomba);
        m.cmd(1, "COMPRAR");
        int bombasNoBaralho = 0; for (auto& k : m.p().baralho) bombasNoBaralho += k->getTipo() == TipoCarta::Bomba;
        m.desconectar(1);
        int depois = 0; for (auto& k : m.p().baralho) depois += k->getTipo() == TipoCarta::Bomba;
        OK(depois == bombasNoBaralho && depois == 1 && m.p().numeroJogadoresVivos == 2, "bombas = vivos - 1 (%d)", depois);
        OK(m.p().fase == Partida::Fase::TurnoNormal && m.vez() == 2, "segue a vez de J2");
    }
}

// ============================ fuzz ============================

static void invariantes(Mundo& m, int partida) {
    if (!m.srv->partidaAtual) return;
    Partida& p = m.p();
    if (!p.emAndamento) return;
    std::set<int> ids;
    int vivos = 0, bombas = 0;
    auto unico = [&](const std::unique_ptr<Carta>& k) { OK(ids.insert(k->getId()).second, "id de carta duplicado %d (partida %d)", k->getId(), partida); };
    for (auto& c : p.jogadores) {
        Jogador& j = c->jogador;
        if (j.estaVivo()) ++vivos; else OK(j.getTamanhoMao() == 0, "morto com cartas");
        for (auto& k : j.getMao()) { unico(k); OK(k->getTipo() != TipoCarta::Bomba, "bomba na mao"); }
    }
    for (auto& k : p.baralho) { unico(k); bombas += k->getTipo() == TipoCarta::Bomba; }
    for (auto& k : p.pilhaDescarte) unico(k);
    for (auto& k : p.pilhaEfeitos) unico(k);
    if (p.bombaEmMao) unico(p.bombaEmMao);
    OK(vivos == p.numeroJogadoresVivos && static_cast<int>(p.ordemTurnos.size()) == vivos, "vivos=%d contador=%d ordem=%zu", vivos, p.numeroJogadoresVivos, p.ordemTurnos.size());
    for (int idx : p.ordemTurnos) OK(p.jogadores[static_cast<size_t>(idx)]->jogador.estaVivo(), "morto na ordem de turnos");
    OK(p.turnosPendentes >= 1, "turnos pendentes %d", p.turnosPendentes);
    if (!p.descartarBombaReinserida)
        OK(bombas + (p.bombaEmMao ? 1 : 0) == vivos - 1, "bombas (%d + %d na mao) != vivos-1 (%d) partida %d", bombas, p.bombaEmMao ? 1 : 0, vivos - 1, partida);
}

static int fuzz(int partidas) {
    std::mt19937 rng(20240607);
    const char* lixo[] = {"", "JOGAR", "JOGAR abc", "JOGAR 1,1", "JOGAR -1", "JOGAR 99999999999999", "JOGAR ,", "JOGAR 1,2,3,4",
                          "COMPRAR x", "DAR", "DAR zz", "POSICAO", "POSICAO -5", "POSICAO 999999", "FOO", "JOGAR_NAO 3",
                          "JOGAR 1 2 3 4 5", "jogar 1", "MESA", "MAO", "   ", "JOGAR 1, 2", "JOGAR 3 x", "JOGAR 3 99 PULAR"};
    int terminadas = 0, desconexoes = 0;
    for (int g = 0; g < partidas; ++g) {
        const int n = 2 + static_cast<int>(rng() % 4);
        Mundo m(n);
        std::set<int> conectados;
        for (int i = 1; i <= n; ++i) conectados.insert(i);

        int passo = 0;
        for (; passo < 30000 && m.srv->jogoIniciado; ++passo) {
            invariantes(m, g);
            if (rng() % 150 == 0 && !conectados.empty()) {  // desconexão abrupta
                auto it = conectados.begin(); std::advance(it, rng() % conectados.size());
                m.desconectar(*it); conectados.erase(it); ++desconexoes;
                continue;
            }
            if (rng() % 4 == 0 && !conectados.empty()) {    // fuzz
                auto it = conectados.begin(); std::advance(it, rng() % conectados.size());
                m.cmd(*it, lixo[rng() % (sizeof(lixo) / sizeof(lixo[0]))]);
                continue;
            }
            Partida& p = m.p();
            switch (p.fase) {
                case Partida::Fase::JanelaNao:
                    if (rng() % 3 == 0) {
                        int id = 1 + static_cast<int>(rng() % n);
                        if (conectados.count(id)) m.cmd(id, "JOGAR_NAO");
                    } else m.tempo();
                    break;
                case Partida::Fase::EscolhendoFavor: {
                    int alvo = p.favorAlvoId;
                    auto& mao = p.buscarPorId(alvo)->jogador.getMao();
                    if (rng() % 2 && !mao.empty() && conectados.count(alvo)) m.cmd(alvo, "DAR " + S(mao[rng() % mao.size()]->getId()));
                    else m.tempo();
                    break;
                }
                case Partida::Fase::ReinserindoBomba:
                    if (rng() % 2 && conectados.count(p.bombaJogadorId)) m.cmd(p.bombaJogadorId, "POSICAO " + S(static_cast<int>(rng() % (p.baralho.size() + 1))));
                    else m.tempo();
                    break;
                case Partida::Fase::TurnoNormal: {
                    const int vez = p.idDaVez();
                    if (!conectados.count(vez)) break;  // não deveria ocorrer
                    auto& mao = p.buscarPorId(vez)->jogador.getMao();
                    if (rng() % 2 == 0 && !mao.empty()) {
                        const Carta& a = *mao[rng() % mao.size()];
                        int alvo = 1 + static_cast<int>(rng() % n);
                        std::ostringstream c;
                        c << "JOGAR " << a.getId();
                        std::vector<int> iguais;
                        for (auto& k : mao) if (k->getTipo() == a.getTipo()) iguais.push_back(k->getId());
                        if (ehGato(a.getTipo()) && iguais.size() >= 2) {
                            size_t k = 2 + rng() % std::min<size_t>(2, iguais.size() - 1);
                            c.str("JOGAR ");
                            for (size_t i = 0; i < k; ++i) c << (i ? "," : "") << iguais[i];
                            c << " " << alvo;
                            if (k == 3) c << " " << tipoParaToken(static_cast<TipoCarta>(rng() % 13));
                        } else if (a.getTipo() == TipoCarta::Favor) c << " " << alvo;
                        m.cmd(vez, c.str());
                    } else m.cmd(vez, "COMPRAR");
                    break;
                }
            }
        }
        invariantes(m, g);
        if (!m.srv->jogoIniciado) ++terminadas;
        else OK(false, "partida %d nao terminou em %d passos", g, passo);
    }
    std::printf("fuzz: %d partidas | terminadas: %d | desconexoes simuladas: %d\n", partidas, terminadas, desconexoes);
    return terminadas;
}

int main(int argc, char** argv) {
    std::cout.setstate(std::ios_base::failbit);  // silencia os logs do servidor ("Jogador x saiu...")
    cenarios();
    std::cout.clear();
    std::printf("cenarios: %d verificacoes, %d falhas\n", verificacoes, falhas);
    std::cout.setstate(std::ios_base::failbit);
    fuzz(argc > 1 ? std::atoi(argv[1]) : 600);
    std::cout.clear();
    std::printf("TOTAL: %d verificacoes, %d falhas\n", verificacoes, falhas);
    return falhas ? 1 : 0;
}
