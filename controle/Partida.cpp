#include "Partida.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "ServidorTCP.hpp"

namespace {

constexpr int kMinJogadores = 2;
constexpr int kMaxJogadores = 5;
constexpr int kCartasIniciais = 7;   // além do Defuse inicial
constexpr int kTotalDefuses = 6;

std::unique_ptr<Carta> criarCarta(TipoCarta tipo) {
    switch (tipo) {
        case TipoCarta::Bomba:        return std::make_unique<Carta>(99, "BOMBA", "Exploding Kitten", tipo);
        case TipoCarta::Desarme:      return std::make_unique<Carta>(0, "DEFUSE", "Salva da bomba", tipo);
        case TipoCarta::Ataque:       return std::make_unique<Carta>(1, "ATACAR", "Termina o turno e o proximo joga 2x", tipo);
        case TipoCarta::Pular:        return std::make_unique<Carta>(2, "PULAR", "Termina o turno sem comprar", tipo);
        case TipoCarta::Favor:        return std::make_unique<Carta>(3, "FAVOR", "Pede uma carta a outro jogador", tipo);
        case TipoCarta::Embaralhar:   return std::make_unique<Carta>(4, "EMBARALHAR", "Embaralha o baralho", tipo);
        case TipoCarta::Futuro:       return std::make_unique<Carta>(5, "FUTURO", "Ve as 3 proximas cartas", tipo);
        case TipoCarta::Nao:          return std::make_unique<Carta>(6, "NAO", "Cancela uma acao", tipo);
        case TipoCarta::GatoAranha:   return std::make_unique<Carta>(7, "GATO1", "Gato normal", tipo);
        case TipoCarta::GatoBarba:    return std::make_unique<Carta>(8, "GATO2", "Gato normal", tipo);
        case TipoCarta::GatoBatata:   return std::make_unique<Carta>(9, "GATO3", "Gato normal", tipo);
        case TipoCarta::GatoMelancia: return std::make_unique<Carta>(10, "GATO4", "Gato normal", tipo);
        case TipoCarta::GatoTaco:     return std::make_unique<Carta>(11, "GATO5", "Gato normal", tipo);
    }
    return nullptr;
}

// Baralho base: tudo exceto Bombas e Defuses (46 cartas).
struct ItemBaralho { TipoCarta tipo; int quantidade; };
const ItemBaralho kBaralhoBase[] = {
    {TipoCarta::Ataque, 4},     {TipoCarta::Pular, 4},      {TipoCarta::Favor, 4},
    {TipoCarta::Embaralhar, 4}, {TipoCarta::Futuro, 5},     {TipoCarta::Nao, 5},
    {TipoCarta::GatoAranha, 4}, {TipoCarta::GatoBarba, 4},  {TipoCarta::GatoBatata, 4},
    {TipoCarta::GatoMelancia, 4}, {TipoCarta::GatoTaco, 4},
};

// Cartas que só existem como reação / automaticamente: não podem ser "jogadas" no turno.
bool ehCartaDeReacao(TipoCarta t) {
    return t == TipoCarta::Desarme || t == TipoCarta::Bomba || t == TipoCarta::Nao;
}

// Cartas com efeito já programado. As demais (Favor, Gatos) são recusadas ao jogar,
// para não travar o turno de ninguém.
// TODO: combinações de gatos entram aqui quando forem implementadas.
bool efeitoImplementado(TipoCarta t) {
    return t == TipoCarta::Ataque || t == TipoCarta::Pular || t == TipoCarta::Embaralhar || t == TipoCarta::Futuro || t == TipoCarta::Favor;
}

std::string minusculo(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int indiceDoTipo(const std::vector<std::unique_ptr<Carta>>& mao, TipoCarta tipo) {
    for (size_t i = 0; i < mao.size(); ++i)
        if (mao[i]->getTipo() == tipo) return static_cast<int>(i);
    return -1;
}

}  // namespace

Partida::Partida(const ListaClientes& clientes)
    : jogadores(clientes), rng(std::random_device{}()) {}

Partida::~Partida() = default;

// =====================================================================
// Preparação
// =====================================================================

void Partida::iniciar(ServidorTCP& servidor) {
    const int n = static_cast<int>(jogadores.size());
    if (n < kMinJogadores || n > kMaxJogadores) {
        servidor.broadcast("ERRO NUMERO_JOGADORES_INVALIDO\n");
        return;
    }

    andamento = true;
    turnosPendentes = 1;
    ordemTurnos.clear();
    for (auto& c : jogadores) ordemTurnos.push_back(c->jogador.getId());

    montarBaralho();

    servidor.broadcast("JOGO_INICIADO\n");
    anunciarTurno(servidor);
}

// Segue a preparação do manual: mão inicial sem bombas/defuses, depois
// bombas (n-1) e defuses restantes entram no baralho.
void Partida::montarBaralho() {
    const int n = static_cast<int>(jogadores.size());

    Baralho base;
    for (const auto& item : kBaralhoBase)
        for (int i = 0; i < item.quantidade; ++i) base.push_back(criarCarta(item.tipo));
    std::shuffle(base.begin(), base.end(), rng);

    // 1 Defuse + 7 cartas aleatórias para cada jogador
    for (auto& c : jogadores) {
        c->jogador.adicionarCartaMao(criarCarta(TipoCarta::Desarme));
        for (int i = 0; i < kCartasIniciais; ++i) {
            c->jogador.adicionarCartaMao(std::move(base.back()));
            base.pop_back();
        }
    }

    // n-1 Exploding Kittens
    for (int i = 0; i < n - 1; ++i) base.push_back(criarCarta(TipoCarta::Bomba));

    // Defuses que sobraram: 6 - n (versão de 2 jogadores: apenas 2 no baralho)
    const int defusesExtras = (n == 2) ? 2 : kTotalDefuses - n;
    for (int i = 0; i < defusesExtras; ++i) base.push_back(criarCarta(TipoCarta::Desarme));

    baralho = std::move(base);
    std::shuffle(baralho.begin(), baralho.end(), rng);
}

// =====================================================================
// Roteador de comandos
// =====================================================================

void Partida::processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor) {
    std::istringstream iss(comando);
    std::string acao;
    iss >> acao;

    if (acao == "MESA") {
        enviar(cliente, obterEstadoMesa(cliente));
        return;
    }
    if (!andamento) return;

    if (!cliente.jogador.estaVivo()) {
        enviar(cliente, "ERRO VOCE_ESTA_ELIMINADO\n");
        return;
    }

    // FASE 1: Reação ao NAO (Qualquer um, exceto quem jogou a carta)
    if (aguardandoReacao) {
        std::string acaoUpper = acao;
        std::transform(acaoUpper.begin(), acaoUpper.end(), acaoUpper.begin(), ::toupper);

        if (acaoUpper == "JOGAR_NAO")  processarRespostaNao(cliente, true, servidor);
        else if (acaoUpper == "PASSO") processarRespostaNao(cliente, false, servidor);
        else                           enviar(cliente, "ERRO AGUARDANDO_RESPOSTA_NAO\n");
        return; // Sai imediatamente, não passa pelas validações abaixo
    }

    // FASE 2: Doador escolhendo a carta para entregar (Apenas o alvo do Favor)
    if (aguardandoEscolhaCarta) {
        if (cliente.jogador.getId() != idJogadorDoador) {
            enviar(cliente, "ERRO AGUARDANDO_DOACAO_DE_CARTA\n");
            return;
        }

        int indice;
        try {
            indice = std::stoi(acao);
        } catch (...) {
            enviar(cliente, "ERRO INDICE_INVALIDO\n");
            return;
        }

        if (indice < 0 || indice >= static_cast<int>(cliente.jogador.getTamanhoMao())) {
            enviar(cliente, "ERRO INDICE_INVALIDO\n");
            return;
        }

        auto origem = obterJogadorPorId(cliente.jogador.getId());
        auto destino = obterJogadorPorId(ordemTurnos.front()); // O autor do turno recebe a carta

        if (origem && destino) {
            const std::string nomeCarta = origem->jogador.getMao()[static_cast<size_t>(indice)]->getNome();
            transferirCarta(indice, origem, destino);
            aguardandoEscolhaCarta = false;
            idJogadorDoador = -1;

            enviar(cliente, "CARTA_DOADA\n");
            enviar(*destino, "RECEBEU " + cliente.jogador.getNome() + " " + nomeCarta + "\n");
            anunciarTurno(servidor); // Retoma o jogo normal
        }
        return;
    }

    // FASE 3: Autor do Favor escolhendo o oponente alvo
    if (aguardandoEscolhaOponente) {
        // VERIFICAÇÃO DO AUTOR: Apenas quem jogou o Favor (o dono do turno) pode escolher o alvo
        if (cliente.jogador.getId() != ordemTurnos.front()) {
            enviar(cliente, "ERRO APENAS_O_AUTOR_PODE_ESCOLHER_O_ALVO\n");
            return;
        }

        const std::string digitado = minusculo(acao);  // o cliente envia em MAIÚSCULAS
        for (int id : ordemTurnos) {
            auto alvo = obterJogadorPorId(id);
            if (alvo && digitado == minusculo(alvo->jogador.getNome())) {
                if (alvo->jogador.getId() == cliente.jogador.getId()) {
                    enviar(cliente, "ERRO ESCOLHA_OUTRO_JOGADOR\n");
                    return;
                }
                if (alvo->jogador.getTamanhoMao() == 0) {
                    enviar(cliente, "ERRO JOGADOR_SEM_CARTAS\n");
                    return;
                }
                aguardandoEscolhaOponente = false;
                idJogadorDoador = alvo->jogador.getId();
                servidor.broadcast("FAVOR " + std::to_string(cliente.jogador.getId()) + " " +
                                   std::to_string(idJogadorDoador) + "\n");
                processarRoubarCarta(alvo, servidor);
                return;
            }
        }
        enviar(cliente, "ERRO JOGADOR_NAO_ENCONTRADO\n");
        return;
    }

    // FASE 4: Validação de Turno normal (Comprar ou Jogar carta)
    // Se o jogo não está esperando Favor ou Reação, valida se é o turno do jogador
    if (ordemTurnos.empty() || ordemTurnos.front() != cliente.jogador.getId()) {
        enviar(cliente, "ERRO NAO_E_SEU_TURNO\n");
        return;
    }

    if (acao == "COMPRAR") {
        comprarCarta(cliente, servidor);
    } else if (acao == "JOGAR") {
        int indice;
        if (!(iss >> indice)) {
            enviar(cliente, "ERRO COMANDO_INVALIDO_USE_NUMEROS\n");
            return;
        }
        jogarCarta(cliente, indice, servidor);
    } else {
        enviar(cliente, "ERRO COMANDO_INVALIDO\n");
    }
}

// =====================================================================
// Turnos
// =====================================================================

// Único ponto que anuncia o turno: TURNO seguido de UMA mesa para cada jogador.
// Todo fluxo de jogada deve terminar chamando isto exatamente uma vez.
void Partida::anunciarTurno(ServidorTCP& servidor) {
    if (!andamento || ordemTurnos.empty()) return;
    servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");
    notificarTodosMesa(servidor);
}

void Partida::passarParaProximo() {
    ordemTurnos.push_back(ordemTurnos.front());
    ordemTurnos.pop_front();
    turnosPendentes = 1;
}

// Termina UM dos turnos do jogador da vez (após comprar ou pular).
// Se ele estava sob ataque (2 turnos), a vez continua com ele.
void Partida::consumirTurno() {
    if (--turnosPendentes <= 0) passarParaProximo();
}

// =====================================================================
// Comprar (fim do turno) e explosão
// =====================================================================

void Partida::comprarCarta(ClienteConectado& cliente, ServidorTCP& servidor) {
    const int id = cliente.jogador.getId();

    if (baralho.empty()) {  // não deveria ocorrer (há bombas suficientes), mas não trava o jogo
        enviar(cliente, "ERRO BARALHO_VAZIO\n");
        consumirTurno();
        anunciarTurno(servidor);
        return;
    }

    auto carta = std::move(baralho.back());
    baralho.pop_back();

    if (carta->getTipo() != TipoCarta::Bomba) {
        const std::string nome = carta->getNome();
        cliente.jogador.adicionarCartaMao(std::move(carta));
        // Só o dono vê qual carta veio; os outros só sabem que ele comprou.
        enviar(cliente, "COMPROU_VOCE " + nome + "\n");
        servidor.broadcast("COMPROU " + std::to_string(id) + "\n");
        consumirTurno();
        anunciarTurno(servidor);
        return;
    }

    // ---- Exploding Kitten ----
    servidor.broadcast("EXPLOSAO " + std::to_string(id) + "\n");

    const int idxDefuse = indiceDoTipo(cliente.jogador.getMao(), TipoCarta::Desarme);
    if (idxDefuse >= 0) {
        // Salvo: o Defuse vai ao descarte e o gatinho volta ao baralho numa posição aleatória.
        // TODO: deixar o jogador escolher a posição, como no manual.
        pilhaDescarte.push_back(cliente.jogador.removerCartaMao(static_cast<size_t>(idxDefuse)));
        std::uniform_int_distribution<size_t> dist(0, baralho.size());
        baralho.insert(baralho.begin() + static_cast<std::ptrdiff_t>(dist(rng)), std::move(carta));
        servidor.broadcast("DEFUSOU " + std::to_string(id) + "\n");
        consumirTurno();
        anunciarTurno(servidor);
    } else {
        // Sem Defuse: o jogador morre e a bomba sai do jogo (a 'carta' é destruída aqui).
        // eliminarJogador cuida de passar a vez e anunciar o turno.
        eliminarJogador(id, Motivo::Explosao, servidor);
    }
}

// =====================================================================
// Jogar carta e janela de reação (NAO)
// =====================================================================

void Partida::jogarCarta(ClienteConectado& cliente, int indice, ServidorTCP& servidor) {
    const auto& mao = cliente.jogador.getMao();
    if (indice < 0 || static_cast<size_t>(indice) >= mao.size()) {
        enviar(cliente, "ERRO INDICE_INVALIDO\n");
        return;
    }

    const TipoCarta tipo = mao[static_cast<size_t>(indice)]->getTipo();
    if (ehCartaDeReacao(tipo)) {
        enviar(cliente, "ERRO CARTA_DE_REACAO_NAO_PODE_SER_JOGADA_ASSIM\n");
        return;
    }
    if (!efeitoImplementado(tipo)) {
        enviar(cliente, "ERRO CARTA_NAO_IMPLEMENTADA\n");
        return;
    }

    auto carta = cliente.jogador.removerCartaMao(static_cast<size_t>(indice));
    const std::string nome = carta->getNome();
    idUltimoAutor = cliente.jogador.getId();
    pilhaEfeitos.push_back(std::move(carta));

    servidor.broadcast("JOGOU " + std::to_string(idUltimoAutor) + " " + nome + "\n");
    abrirJanelaReacao(servidor);
}

// Todos os outros jogadores vivos podem responder, um de cada vez (ninguém vê a mão de ninguém).
void Partida::abrirJanelaReacao(ServidorTCP& servidor) {
    aguardandoReacao = true;
    filaRespostaNao.clear();
    for (int id : ordemTurnos)
        if (id != idUltimoAutor) filaRespostaNao.push_back(id);

    if (filaRespostaNao.empty()) resolverEfeitos(servidor);
    else                         perguntarAoPrimeiro(servidor);
}

void Partida::perguntarAoPrimeiro(ServidorTCP&) {
    if (filaRespostaNao.empty()) return;
    auto alvo = obterJogadorPorId(filaRespostaNao.front());
    if (!alvo) return;

    const std::string nomeCarta = pilhaEfeitos.empty() ? "CARTA" : pilhaEfeitos.back()->getNome();
    enviar(*alvo, "PERGUNTA_NAO " + std::to_string(idUltimoAutor) + " " + nomeCarta + "\n");
}

void Partida::processarRespostaNao(ClienteConectado& cliente, bool querJogar, ServidorTCP& servidor) {
    const int id = cliente.jogador.getId();

    if (filaRespostaNao.empty() || filaRespostaNao.front() != id) {
        enviar(cliente, "ERRO NAO_E_SUA_VEZ_DE_REAGIR\n");
        return;
    }

    if (querJogar) {
        const int idx = indiceDoTipo(cliente.jogador.getMao(), TipoCarta::Nao);
        if (idx < 0) {
            // Continua a vez dele: repete a pergunta em vez de tratar como PASSO.
            enviar(cliente, "ERRO VOCE_NAO_TEM_A_CARTA_NAO\n");
            perguntarAoPrimeiro(servidor);
            return;
        }

        pilhaEfeitos.push_back(cliente.jogador.removerCartaMao(static_cast<size_t>(idx)));
        idUltimoAutor = id;
        servidor.broadcast("JOGOU_NAO " + std::to_string(id) + "\n");

        // O NAO também pode ser cancelado: pergunta de novo a todos os outros.
        filaRespostaNao.clear();
        for (int outro : ordemTurnos)
            if (outro != id) filaRespostaNao.push_back(outro);

        if (filaRespostaNao.empty()) resolverEfeitos(servidor);
        else                         perguntarAoPrimeiro(servidor);
        return;
    }

    // PASSO
    filaRespostaNao.pop_front();
    if (filaRespostaNao.empty()) resolverEfeitos(servidor);
    else                         perguntarAoPrimeiro(servidor);
}

// Ninguém mais reage: a pilha é resolvida.
// Quantidade par de cartas = a ação original foi cancelada por um número ímpar de NAOs.
void Partida::resolverEfeitos(ServidorTCP& servidor) {
    if (!aguardandoReacao || pilhaEfeitos.empty()) return;

    const bool cancelado = (pilhaEfeitos.size() % 2 == 0);
    const TipoCarta tipoOriginal = pilhaEfeitos.front()->getTipo();

    descartarEfeitosPendentes();

    if (cancelado) {
        servidor.broadcast("CANCELADO\n");
        anunciarTurno(servidor);
        return;
    }
    aplicarEfeito(tipoOriginal, servidor);
}

void Partida::aplicarEfeito(TipoCarta tipo, ServidorTCP& servidor) {
    switch (tipo) {
        case TipoCarta::Ataque:
            // Encerra o(s) turno(s) de quem jogou; o próximo joga dois (regra do manual).
            passarParaProximo();
            turnosPendentes = 2;
            break;

        case TipoCarta::Pular:
            consumirTurno();
            break;

        case TipoCarta::Embaralhar:
            std::shuffle(baralho.begin(), baralho.end(), rng);
            servidor.broadcast("EMBARALHOU\n");
            break;

        case TipoCarta::Futuro: {
            // Topo do baralho = back(); mostra as 3 primeiras, só para quem jogou.
            std::string msg = "FUTURO";
            const size_t n = std::min<size_t>(3, baralho.size());
            for (size_t i = 0; i < n; ++i) msg += " " + baralho[baralho.size() - 1 - i]->getNome();
            if (auto autor = obterJogadorPorId(ordemTurnos.front())) enviar(*autor, msg + "\n");
            break;
        }
        
        case TipoCarta::Favor: {
            // Se ninguém tem carta para dar, o Favor não tem efeito (evita travar o autor).
            bool alguemTemCarta = false;
            for (int id : ordemTurnos)
                if (id != ordemTurnos.front()) {
                    auto o = obterJogadorPorId(id);
                    if (o && o->jogador.getTamanhoMao() > 0) { alguemTemCarta = true; break; }
                }
            if (!alguemTemCarta) {
                servidor.broadcast("FAVOR_SEM_EFEITO\n");
                break;  // cai no anunciarTurno() do fim da função
            }

            aguardandoEscolhaOponente = true;
            tipoDoacao = tipoDoacao::Favor;

            // O dono do turno é obrigatoriamente o autor da carta original.
            if (auto autor = obterJogadorPorId(ordemTurnos.front()))
                enviar(*autor, "ESCOLHER_ALVO\n");
            return;  // não anuncia o turno: o jogo espera a escolha do alvo e da carta
        }
        default:
            break;  // não chega aqui: jogarCarta recusa cartas sem efeito implementado
    }
    anunciarTurno(servidor);
}
void Partida::transferirCarta(int posicaoCarta, std::shared_ptr<ClienteConectado> origem, std::shared_ptr<ClienteConectado> destino) {
    auto cartaTransferida = origem->jogador.removerCartaMao(posicaoCarta);
    destino->jogador.adicionarCartaMao(std::move(cartaTransferida));
}

void Partida::processarRoubarCarta(std::shared_ptr<ClienteConectado> alvo, ServidorTCP& servidor) {
    auto autor = obterJogadorPorId(ordemTurnos.front()); // O autor é o dono do turno atual
    
    switch (tipoDoacao) {
        case tipoDoacao::Favor: {
            aguardandoEscolhaCarta = true;
            enviar(*alvo, "ESCOLHER_CARTA " + std::to_string(ordemTurnos.front()) + "\n");
            break;
        }
        case tipoDoacao::combo2: {
            if (alvo->jogador.getTamanhoMao() > 0) {
                std::uniform_int_distribution<size_t> dist(0, alvo->jogador.getTamanhoMao() - 1);
                const size_t idx = dist(rng);
                transferirCarta(idx, alvo, autor);
            }
            anunciarTurno(servidor);
            break;
        }
        case tipoDoacao::combo3: {
            aguardandoEscolhaTipoCarta = true;
            if (autor) enviar(*autor, "ESCOLHA O TIPO DE CARTA PARA ROUBAR\n");
            break;
        }
        default:
            break;
    }
    tipoDoacao = tipoDoacao::nothing;
}
void Partida::descartarEfeitosPendentes() {
    for (auto& c : pilhaEfeitos) pilhaDescarte.push_back(std::move(c));
    pilhaEfeitos.clear();
    filaRespostaNao.clear();
    aguardandoReacao = false;
}

// =====================================================================
// Eliminação e fim de jogo
// =====================================================================

void Partida::removerJogador(int idJogador, ServidorTCP& servidor) {
    if (!andamento) return;
    eliminarJogador(idJogador, Motivo::Desconexao, servidor);
}

// Usado tanto na explosão quanto na desconexão: tira o jogador da ordem,
// marca como morto, descarta a mão e deixa o jogo num estado consistente.
void Partida::eliminarJogador(int idJogador, Motivo motivo, ServidorTCP& servidor) {
    auto it = std::find(ordemTurnos.begin(), ordemTurnos.end(), idJogador);
    if (it == ordemTurnos.end()) return;  // já estava fora

    const bool eraDaVez = (it == ordemTurnos.begin());
    ordemTurnos.erase(it);

    if (auto alvo = obterJogadorPorId(idJogador)) {
        alvo->jogador.eliminar();
        while (alvo->jogador.getTamanhoMao() > 0)
            pilhaDescarte.push_back(alvo->jogador.removerCartaMao(0));
    }

    // Quem sai sem explodir leva uma bomba junto: o baralho mantém (vivos - 1) bombas.
    if (motivo == Motivo::Desconexao) removerUmaBombaDoBaralho();

    servidor.broadcast("MORREU " + std::to_string(idJogador) + "\n");
    if (verificarFimDeJogo(servidor)) return;

    if (eraDaVez) {
        // A vez passa para quem já está em front(). Cancela qualquer reação pendente
        // e zera os turnos pendentes (senão o próximo herdaria os turnos de um ataque).
        descartarEfeitosPendentes();
        turnosPendentes = 1;
        anunciarTurno(servidor);
    } else if (aguardandoReacao) {
        const bool eraOPrimeiro = !filaRespostaNao.empty() && filaRespostaNao.front() == idJogador;
        filaRespostaNao.erase(std::remove(filaRespostaNao.begin(), filaRespostaNao.end(), idJogador),
                              filaRespostaNao.end());
        if (filaRespostaNao.empty())  resolverEfeitos(servidor);
        else if (eraOPrimeiro)        perguntarAoPrimeiro(servidor);  // só ao novo primeiro, não a todos
    } else {
        // Se estava no meio de um Favor, o autor precisa voltar ao turno normal;
        // TURNO + mesa também limpam o estado dos clientes.
        anunciarTurno(servidor);
    }
    aguardandoEscolhaOponente = false;
    aguardandoEscolhaCarta = false;
    idJogadorDoador = -1;
}

bool Partida::verificarFimDeJogo(ServidorTCP& servidor) {
    if (ordemTurnos.size() > 1) return false;

    andamento = false;
    if (!ordemTurnos.empty())
        servidor.broadcast("FIM_DE_JOGO VENCEDOR " + std::to_string(ordemTurnos.front()) + "\n");
    return true;
}

void Partida::removerUmaBombaDoBaralho() {
    auto it = std::find_if(baralho.begin(), baralho.end(),
                           [](const std::unique_ptr<Carta>& c) { return c->getTipo() == TipoCarta::Bomba; });
    if (it != baralho.end()) baralho.erase(it);
}

// =====================================================================
// Utilidades e estado da mesa
// =====================================================================

std::shared_ptr<ClienteConectado> Partida::obterJogadorPorId(int idJogador) const {
    for (const auto& c : jogadores)
        if (c->jogador.getId() == idJogador) return c;
    return nullptr;
}

void Partida::enviar(const ClienteConectado& cliente, const std::string& msg) {
    if (cliente.socket >= 0) ServidorTCP::enviarTudo(cliente.socket, msg);
}

void Partida::notificarTodosMesa(ServidorTCP&) {
    for (const auto& c : jogadores) enviar(*c, obterEstadoMesa(*c));
}

// Segmentos separados por '|'. O ÚLTIMO (histórico do descarte) o cliente só mostra sob demanda.
std::string Partida::obterEstadoMesa(const ClienteConectado& cliente) const {
    std::ostringstream oss;
    oss << "MESA_ESTADO ";

    oss << "Baralho: " << baralho.size() << " cartas | ";
    oss << "Descarte: " << pilhaDescarte.size() << " cartas | ";

    oss << "Vez de: ";
    if (ordemTurnos.empty()) {
        oss << "-";
    } else if (auto atual = obterJogadorPorId(ordemTurnos.front())) {
        oss << atual->jogador.getNome() << " (" << turnosPendentes << " turno(s) restante(s))";
    }
    oss << " | ";

    oss << "Oponentes: ";
    for (const auto& c : jogadores) {
        if (c->jogador.getId() != cliente.jogador.getId() && c->jogador.estaVivo())
            oss << c->jogador.getNome() << " (" << c->jogador.getTamanhoMao() << " cartas)  ";
    }
    oss << "| ";

    oss << "Sua mao: ";
    const auto& mao = cliente.jogador.getMao();
    if (mao.empty()) oss << "VAZIA";
    for (size_t i = 0; i < mao.size(); ++i) oss << "[" << i << "] " << mao[i]->getNome() << "  ";
    oss << "| ";

    oss << "Historico_Descarte: ";
    if (pilhaDescarte.empty()) oss << "Vazio";
    for (const auto& c : pilhaDescarte) oss << c->getNome() << " ";
    oss << "\n";

    return oss.str();
}