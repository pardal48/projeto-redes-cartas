#include "Partida.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "ServidorTCP.hpp"

namespace {

constexpr int kMinJogadores = 2;  // número mínimo de jogadores
constexpr int kMaxJogadores = 5;  // número máximo de jogadores
constexpr int kCartasIniciais = 7;  // número de cartas em uma mão inicial, além do Defuse
constexpr int kTotalDefuses = 6;  // número total de defuses no jogo

std::unique_ptr<Carta> criarCarta(TipoCarta tipo) {  // função de criar uma carta com base nos tipos
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
        case TipoCarta::Combo2:       return std::make_unique<Carta>(12, "COMBO2", "Combo de 2 cartas", tipo);
        case TipoCarta::Combo3:       return std::make_unique<Carta>(13, "COMBO3", "Combo de 3 cartas", tipo);
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


// Cartas não podem ser "jogadas" por si só: ou combo(gatos) ou como reação(bomba, desarme).
bool ehCartaNaoJogavel(TipoCarta t) {
    return t == TipoCarta::Desarme || t == TipoCarta::Bomba || t == TipoCarta::Nao|| t == TipoCarta::GatoAranha || t == TipoCarta::GatoBarba || t == TipoCarta::GatoBatata ||
           t == TipoCarta::GatoMelancia || t == TipoCarta::GatoTaco;
}




// Cartas com efeito já programado. As demais (Gatos, desarme, não) são recusadas ao jogar,
// para não travar o turno de ninguém.
bool efeitoImplementado(TipoCarta t) {
    return t == TipoCarta::Ataque || t == TipoCarta::Pular || t == TipoCarta::Embaralhar || t == TipoCarta::Futuro || t == TipoCarta::Favor;
}

// transforma uma string em minúsculas
std::string minusculo(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

//retorna o índice da carta do tipo especificado na mão, ou -1 se não houver
int indiceDoTipo(const std::vector<std::unique_ptr<Carta>>& mao, TipoCarta tipo) {
    for (size_t i = 0; i < mao.size(); ++i)
        if (mao[i]->getTipo() == tipo) return static_cast<int>(i);
    return -1;
}

}  // namespace

// =====================================================================
Partida::Partida(const ListaClientes& clientes)
    : jogadores(clientes), rng(std::random_device{}()) {}

Partida::~Partida() = default;





// =====================================================================
// Preparação
// =====================================================================
//inicia a partida, montando o baralho e a ordem dos turnos
void Partida::iniciar(ServidorTCP& servidor) {
    const int n = static_cast<int>(jogadores.size());
    if (n < kMinJogadores || n > kMaxJogadores) {
        servidor.broadcast("ERRO NUMERO_JOGADORES_INVALIDO\n");
        return;
    }

    andamento = true;
    turnosPendentes = 1;
    // Configura a ordem dos turnos
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
        for (int i = 0; i < item.quantidade; ++i) base.push_back(criarCarta(item.tipo));  //adiciona as cartas no baralho
    std::shuffle(base.begin(), base.end(), rng);  //embaralha


    // distribuí as cartas para os jogadores:1 Defuse + 7 cartas aleatórias para cada jogador
    for (auto& c : jogadores) {
        c->jogador.adicionarCartaMao(criarCarta(TipoCarta::Desarme));
        for (int i = 0; i < kCartasIniciais; ++i) {
            c->jogador.adicionarCartaMao(std::move(base.back()));
            base.pop_back();
        }
    }


    // adiciona número de jogadores - 1 Exploding Kittens
    for (int i = 0; i < n - 1; ++i) base.push_back(criarCarta(TipoCarta::Bomba));


    // adiciona ao baralho os Defuses que sobraram: 6 - n (versão de 2 jogadores: apenas 2 no baralho)
    const int defusesExtras = (n == 2) ? 2 : kTotalDefuses - n;
    for (int i = 0; i < defusesExtras; ++i) base.push_back(criarCarta(TipoCarta::Desarme));

    baralho = std::move(base);
    std::shuffle(baralho.begin(), baralho.end(), rng);
}





// =====================================================================
// Roteador de comandos
// =====================================================================
// função que processa os comandos recebidos do cliente, chamando as funções apropriadas
void Partida::processarComando(ClienteConectado& cliente, const std::string& comando, ServidorTCP& servidor) {
    std::istringstream iss(comando);
    std::string acao;
    iss >> acao;

    if (acao == "MESA") {
        enviar(cliente, obterEstadoMesa(cliente));
        return;
    }
    if (!andamento) return;

    if (!cliente.jogador.estaVivo()) {  //jogadores eliminados não podem jogar
        enviar(cliente, "ERRO VOCE_ESTA_ELIMINADO\n");
        return;
    }

    if (aguardandoReacao) {  //aguarda a resposta de um jogador à pergunta do NAO
        std::string acaoUpper = acao;
        std::transform(acaoUpper.begin(), acaoUpper.end(), acaoUpper.begin(), ::toupper);

        if (acaoUpper == "JOGAR_NAO")  processarRespostaNao(cliente, true, servidor);
        else if (acaoUpper == "PASSO") processarRespostaNao(cliente, false, servidor);
        else                           enviar(cliente, "ERRO AGUARDANDO_RESPOSTA_NAO\n");
        return;
    }

    if(aguardandoEscolhaTipoCarta){  //aguarda a escolha do tipo de carta a ser doada (Combo3)
        if (cliente.jogador.getId() != ordemTurnos.front()) {
            enviar(cliente, "ERRO NAO_E_SEU_TURNO\n");
            return;
        }

        std::string tipoCartaStr = minusculo(acao);
        TipoCarta tipoEscolhido;  //processa o tipo escolhido
        if(tipoCartaStr == "favor"){
            tipoEscolhido = TipoCarta::Favor;
        }else if(tipoCartaStr == "ataque"){
            tipoEscolhido = TipoCarta::Ataque;
        }else if(tipoCartaStr == "pular"){
            tipoEscolhido = TipoCarta::Pular;
        }else if(tipoCartaStr == "embaralhar"){
            tipoEscolhido = TipoCarta::Embaralhar;
        }else if(tipoCartaStr == "futuro"){
            tipoEscolhido = TipoCarta::Futuro;
        }else if(tipoCartaStr == "gato1"){
            tipoEscolhido = TipoCarta::GatoAranha;
        }else if(tipoCartaStr == "gato2"){
            tipoEscolhido = TipoCarta::GatoBarba;
        }else if(tipoCartaStr == "gato3"){
            tipoEscolhido = TipoCarta::GatoBatata;
        }else if(tipoCartaStr == "gato4"){
            tipoEscolhido = TipoCarta::GatoMelancia;
        }else if(tipoCartaStr == "gato5"){
            tipoEscolhido = TipoCarta::GatoTaco;
        }else if(tipoCartaStr == "nao"){
            tipoEscolhido = TipoCarta::Nao;
        }else if(tipoCartaStr == "defuse"){
            tipoEscolhido = TipoCarta::Desarme;
        }else{
            enviar(cliente, "ERRO TIPO_INVALIDO\n");
            return;
        }
        auto origem = obterJogadorPorId(idJogadorDoador);
        auto destino = obterJogadorPorId(cliente.jogador.getId());

        if (origem && destino) {
            int indice = indiceDoTipo(origem->jogador.getMao(), tipoEscolhido);
            if (indice == -1) {  //se o alvo não tiver o tipo de carta escolhido, a doação não tem efeito
                enviar(cliente, "ERRO NAO_TEM_CARTA_DO_TIPO\n");

                aguardandoEscolhaTipoCarta = false;
                idJogadorDoador = -1;
                servidor.broadcast("FAVOR_SEM_EFEITO\n");
                anunciarTurno(servidor);
                return;
            }

            const std::string nomeCarta = origem->jogador.getMao()[static_cast<size_t>(indice)]->getNome();
            transferirCarta(indice, origem, destino);
            aguardandoEscolhaTipoCarta = false;
            idJogadorDoador = -1;


            enviar(*origem, "CARTA_DOADA\n");
            enviar(cliente, "RECEBEU " + origem->jogador.getNome() + " " + nomeCarta + "\n");
            anunciarTurno(servidor);  //efeito acaba
        }
        return;
    }
    if (aguardandoEscolhaCarta) {  //aguarda a escolha da carta a ser doada (Favor)
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
        auto destino = obterJogadorPorId(ordemTurnos.front());

        if (origem && destino) {
            const std::string nomeCarta = origem->jogador.getMao()[static_cast<size_t>(indice)]->getNome();
            transferirCarta(indice, origem, destino);
            aguardandoEscolhaCarta = false;
            idJogadorDoador = -1;

            enviar(cliente, "CARTA_DOADA\n");
            enviar(*destino, "RECEBEU " + cliente.jogador.getNome() + " " + nomeCarta + "\n");
            anunciarTurno(servidor);  //efeito acaba
        }
        return;
    }


    if (aguardandoEscolhaOponente) {

        // apenas quem jogou o Favor ou Combo (o dono do turno) pode escolher o alvo
        if (cliente.jogador.getId() != ordemTurnos.front()) {
            enviar(cliente, "ERRO APENAS_O_AUTOR_PODE_ESCOLHER_O_ALVO\n");
            return;
        }

        const std::string digitado = minusculo(acao);
        for (int id : ordemTurnos) {  // verifica se o nome digitado corresponde a algum jogador vivo
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
                switch(tipoDoacao){  //notifica o efeito
                    case tipoDoacao::Favor:
                        servidor.broadcast("FAVOR " + std::to_string(cliente.jogador.getId()) + " " +
                                   std::to_string(idJogadorDoador) + "\n");
                        break;
                    case tipoDoacao::combo2:
                        servidor.broadcast("COMBO2 " + std::to_string(cliente.jogador.getId()) + " " +
                                   std::to_string(idJogadorDoador) + "\n");
                        break;
                    case tipoDoacao::combo3:
                        servidor.broadcast("COMBO3 " + std::to_string(cliente.jogador.getId()) + " " +
                                   std::to_string(idJogadorDoador) + "\n");
                        break;
                    default:
                        break;
                }
                processarRoubarCarta(alvo, servidor);  //processa o roubo da carta
                return;
            }
        }
        enviar(cliente, "ERRO JOGADOR_NAO_ENCONTRADO\n");
        return;
    }


    if (ordemTurnos.empty() || ordemTurnos.front() != cliente.jogador.getId()) {  //apenas o jogador da vez pode jogar
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
    } else if (acao == "COMBO"){
        std::istringstream iss2(comando.substr(5));
        jogarCombo(cliente, iss2, servidor);
    } else {
        enviar(cliente, "ERRO COMANDO_INVALIDO\n");
    }
}







void Partida::anunciarTurno(ServidorTCP& servidor) {
    if (!andamento || ordemTurnos.empty()) return;
// Lógica de gerenciar turnos pendentes
        // Passa pro próximo da fila
    servidor.broadcast("TURNO " + std::to_string(ordemTurnos.front()) + "\n");
    notificarTodosMesa(servidor);
}

//passa para o próximo jogador da fila de turnos
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



//compra carta, verificando se a carta comprada é uma bomba
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
// Jogar carta ou combo e janela de reação (NAO)
// =====================================================================
//joga uma carta da mão do jogador da vez, verificando se a carta é jogável
void Partida::jogarCarta(ClienteConectado& cliente, int indice, ServidorTCP& servidor) {
    const auto& mao = cliente.jogador.getMao();
    if (indice < 0 || static_cast<size_t>(indice) >= mao.size()) {
        enviar(cliente, "ERRO INDICE_INVALIDO\n");
        return;
    }

    const TipoCarta tipo = mao[static_cast<size_t>(indice)]->getTipo();
    if (ehCartaNaoJogavel(tipo)) {
        enviar(cliente, "ERRO CARTA_NAO_PODE_SER_JOGADA_ASSIM\n");
        return;
    }
    if (!efeitoImplementado(tipo)) {  //todas as cartas tradicionais do jogo já estão implementadas
        enviar(cliente, "ERRO CARTA_NAO_IMPLEMENTADA\n");
        return;
    }

    // remove a carta da mão e coloca na pilha de efeitos, para que os outros jogadores possam reagir
    auto carta = cliente.jogador.removerCartaMao(static_cast<size_t>(indice));
    const std::string nome = carta->getNome();
    idUltimoAutor = cliente.jogador.getId();
    pilhaEfeitos.push_back(std::move(carta));

    servidor.broadcast("JOGOU " + std::to_string(idUltimoAutor) + " " + nome + "\n");
    abrirJanelaReacao(servidor);  //abre a janela de reação para os outros jogadores reagirem com a carta NAO
}
//joga um combo de cartas da mão do jogador da vez, verificando se as cartas são do mesmo tipo e se o combo é válido
void Partida::jogarCombo(ClienteConectado& cliente, std::istringstream& input, ServidorTCP& servidor) {
    const auto& mao = cliente.jogador.getMao();
    std::vector<int> indices;
    int idxEntrada;

    while (input >> idxEntrada) {  //indentifica as cartas que fazem parte do combo, verificando se os índices são válidos
        if (idxEntrada < 0 || static_cast<size_t>(idxEntrada) >= mao.size()) {
            enviar(cliente, "ERRO INDICE_INVALIDO\n");
            return;
        }
        indices.push_back(idxEntrada);
    }
    std::vector<int> indicesOrdenados = indices;
    std::sort(indicesOrdenados.begin(), indicesOrdenados.end());
    for (size_t i = 1; i < indicesOrdenados.size(); ++i) {
        if (indicesOrdenados[i] == indicesOrdenados[i - 1]) {
            enviar(cliente, "ERRO INDICE_INVALIDO\n");
            return;
        }
    }
    if (indices.size() < 2 || indices.size() > 3) {  //combos devem ter 2 ou 3 cartas
        enviar(cliente, "ERRO COMBO_INVALIDO\n");
        return;
    }


    // Valida se todas as cartas do combo são do mesmo tipo
    const TipoCarta tipo = mao[static_cast<size_t>(indices[0])]->getTipo();
    for (size_t i = 0; i < indices.size(); ++i) {
        if (mao[static_cast<size_t>(indices[i])]->getTipo() != tipo) {
            enviar(cliente, "ERRO CARTAS_NAO_SAO_DO_MESMO_TIPO\n");
            return;
        }
    }

    idUltimoAutor = cliente.jogador.getId();
    servidor.broadcast(std::to_string(idUltimoAutor) + " JOGOU COMBO DE");

    //remove as cartas da mão do jogador em ordem decrescente para não mudar o indíce das outras cartas do combo
    for (auto it = indicesOrdenados.rbegin(); it != indicesOrdenados.rend(); ++it) {
        auto carta = cliente.jogador.removerCartaMao(static_cast<size_t>(*it));
        const std::string nome = carta->getNome();


        pilhaDescarte.push_back(std::move(carta));  //adiciona as cartas do combo na pilha de descarte
        servidor.broadcast(" " + nome);
    }
    servidor.broadcast("\n");

    switch (indices.size()) {  //os combos foram implementados como se fossem cartas que nunca estarão no baralho ou na mão de um jogador.
        //então, a carta de combo correspondente é adicionada na pilha de efeitos, para que os outros jogadores possam reagir
        case 2:
            pilhaEfeitos.push_back(criarCarta(TipoCarta::Combo2));
            break;
        case 3:
            pilhaEfeitos.push_back(criarCarta(TipoCarta::Combo3));
            break;
        default:
            break;
    }

    abrirJanelaReacao(servidor);  //abre a janela de reação para os outros jogadores reagirem com a carta NAO
}

// Todos os outros jogadores vivos podem responder, um de cada vez (ninguém vê a mão de ninguém).
void Partida::abrirJanelaReacao(ServidorTCP& servidor) {
    aguardandoReacao = true;
    filaRespostaNao.clear();
    for (int id : ordemTurnos)
        if (id != idUltimoAutor) filaRespostaNao.push_back(id);

    if (filaRespostaNao.empty()) resolverEfeitos(servidor);  // se não houver outros jogadores, resolve imediatamente o efeito
    else                         perguntarAoPrimeiro(servidor);
}

// Envia PERGUNTA_NAO só para o jogador que está na frente da fila de reação.
void Partida::perguntarAoPrimeiro(ServidorTCP&) {
    if (filaRespostaNao.empty()) return;
    auto alvo = obterJogadorPorId(filaRespostaNao.front());
    if (!alvo) return;

    const std::string nomeCarta = pilhaEfeitos.empty() ? "CARTA" : pilhaEfeitos.back()->getNome();
    enviar(*alvo, "PERGUNTA_NAO " + std::to_string(idUltimoAutor) + " " + nomeCarta + "\n");
}

// Trata a resposta (JOGAR_NAO ou PASSO) de quem está sendo consultado.
// resposta fora de hora (erro), JOGAR_NAO e PASSO.
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
// cada NAO inverte o resultado, então basta contar as cartas da pilha.
// Quantidade par de cartas = a ação original foi cancelada por um número ímpar de NAOs.
void Partida::resolverEfeitos(ServidorTCP& servidor) {
    if (!aguardandoReacao || pilhaEfeitos.empty()) return;

    const bool cancelado = (pilhaEfeitos.size() % 2 == 0);
    const TipoCarta tipoOriginal = pilhaEfeitos.front()->getTipo();

    descartarEfeitosPendentes();

    //numero par de NAOs cancela acao original
    // Mover os ponteiros com std::move de forma limpa para a pilha de descarte
    if (cancelado) {
        servidor.broadcast("CANCELADO\n");
        anunciarTurno(servidor);
        return;
    }
    aplicarEfeito(tipoOriginal, servidor);
}

// Executa o efeito da carta que sobreviveu às reações.
// duas formas de sair do switch
//   break  -> o efeito acabou: cai no anunciarTurno() do fim da função.
//   return -> o efeito precisa de mais escolhas do jogador:
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

        // Favor e combos
        //
        // Se ninguém tem carta para dar, o efeito é descartado;
        // Senão, marca que o servidor espera a escolha do oponente, guarda qual efeito está em andamento (tipoDoacao) e pede o alvo ao autor.
        case TipoCarta::Favor: {  //o alvo escolhe a carta a ser doada
            bool alguemTemCarta = false;
            for (int id : ordemTurnos)
                if (id != ordemTurnos.front()) {
                    auto o = obterJogadorPorId(id);
                    if (o && o->jogador.getTamanhoMao() > 0) { alguemTemCarta = true; break; }
                }
            if (!alguemTemCarta) {
                servidor.broadcast("FAVOR_SEM_EFEITO\n");
                break;
            }

            aguardandoEscolhaOponente = true;
            tipoDoacao = tipoDoacao::Favor;

            if (auto autor = obterJogadorPorId(ordemTurnos.front()))
                enviar(*autor, "ESCOLHER_ALVO FAVOR\n");
            return;
        }
        case TipoCarta::Combo2: {  //a carta a ser doada é escolhida aleatoriamente pelo servidor, entre as cartas do alvo
            bool alguemTemCarta = false;
            for (int id : ordemTurnos)
                if (id != ordemTurnos.front()) {
                    auto o = obterJogadorPorId(id);
                    if (o && o->jogador.getTamanhoMao() > 0) { alguemTemCarta = true; break; }
                }
            if (!alguemTemCarta) {
                servidor.broadcast("COMBO_SEM_EFEITO\n");
                break;
            }

            aguardandoEscolhaOponente = true;
            tipoDoacao = tipoDoacao::combo2;

            if (auto autor = obterJogadorPorId(ordemTurnos.front()))
                enviar(*autor, "ESCOLHER_ALVO COMBO2\n");
            return;
        }
        case TipoCarta::Combo3: {  //o autor escolhe o tipo de carta a ser doada
            bool alguemTemCarta = false;
            for (int id : ordemTurnos)
                if (id != ordemTurnos.front()) {
                    auto o = obterJogadorPorId(id);
                    if (o && o->jogador.getTamanhoMao() > 0) { alguemTemCarta = true; break; }
                }
            if (!alguemTemCarta) {
                servidor.broadcast("COMBO_SEM_EFEITO\n");
                break;
            }

            aguardandoEscolhaOponente = true;
            tipoDoacao = tipoDoacao::combo3;

            if (auto autor = obterJogadorPorId(ordemTurnos.front()))
                enviar(*autor, "ESCOLHER_ALVO COMBO3\n");
            return;
        }
        default:
            break;
    }
    anunciarTurno(servidor);
}
//transfere uma carta da mão de um jogador para outro, removendo a carta da mão do jogador de origem e adicionando à mão do jogador de destino
void Partida::transferirCarta(int posicaoCarta, std::shared_ptr<ClienteConectado> origem, std::shared_ptr<ClienteConectado> destino) {
    auto cartaTransferida = origem->jogador.removerCartaMao(posicaoCarta);
    destino->jogador.adicionarCartaMao(std::move(cartaTransferida));
}

//favor ou combos:
// autor já escolheu o alvo, agora o alvo escolhe a carta a ser doada (Favor) ou o servidor escolhe aleatoriamente (Combo2) ou o autor escolhe o tipo de carta a ser doada (Combo3)
void Partida::processarRoubarCarta(std::shared_ptr<ClienteConectado> alvo, ServidorTCP& servidor) {
    if (!alvo) return;
    auto autor = obterJogadorPorId(ordemTurnos.front());
    if (!autor) return;

    switch (tipoDoacao) {
        case tipoDoacao::Favor: {
            aguardandoEscolhaCarta = true;
            // espera o alvo digitar o número da carta (fase aguardandoEscolhaCarta)
            enviar(*alvo, "ESCOLHER_CARTA " + std::to_string(ordemTurnos.front()) + "\n");
            break;
        }
        case tipoDoacao::combo2: {
            if (alvo->jogador.getTamanhoMao() == 0) {
                anunciarTurno(servidor);
                break;
            }
            // Sorteia uma posição de 0 a (tamanho - 1) e rouba essa carta.
            std::uniform_int_distribution<size_t> dist(0, alvo->jogador.getTamanhoMao() - 1);
            const size_t idx = dist(rng);
            transferirCarta(static_cast<int>(idx), alvo, autor);

            std::string nomeCarta = "uma carta";
            if (!autor->jogador.getMao().empty()) {
                nomeCarta = autor->jogador.getMao().back()->getNome();
            }

            enviar(*alvo, "CARTA_DOADA\n");
            enviar(*autor, "RECEBEU " + alvo->jogador.getNome() + " " + nomeCarta + "\n");
            anunciarTurno(servidor);  // terminou o efeito
            break;
        }
        case tipoDoacao::combo3: {
            aguardandoEscolhaTipoCarta = true;
            if (autor) enviar(*autor, "ESCOLHA_TIPO_CARTA\n");  // espera o autor digitar o tipo (fase aguardandoEscolhaTipoCarta)
            break;
        }
        default:
            break;
    }
}
// Fecha a janela de reação e manda as cartas reais para o descarte
// Combo2/Combo3 não vão para o descarte porque são cartas "virtuais":
// as cartas reais do combo foram descartadas direto em jogarCombo.
void Partida::descartarEfeitosPendentes() {
    for (auto& c : pilhaEfeitos)
        if(c->getTipo() != TipoCarta::Combo2 && c->getTipo() != TipoCarta::Combo3) pilhaDescarte.push_back(std::move(c));
    pilhaEfeitos.clear();
    filaRespostaNao.clear();
    aguardandoReacao = false;
}





// =====================================================================
// Eliminação e fim de jogo
// =====================================================================
// Chamado pelo servidor quando um cliente desconecta no meio da partida.
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
        descartarEfeitosPendentes();
        turnosPendentes = 1;

        // Como o autor da vez morreu/desconectou, cancelamos a escolha pendente
        aguardandoEscolhaOponente = false;
        aguardandoEscolhaCarta = false;
        aguardandoEscolhaTipoCarta = false;
        idJogadorDoador = -1;
        anunciarTurno(servidor);
    } else if (aguardandoReacao) {
        const bool eraOPrimeiro = !filaRespostaNao.empty() && filaRespostaNao.front() == idJogador;
        filaRespostaNao.erase(std::remove(filaRespostaNao.begin(), filaRespostaNao.end(), idJogador),
                              filaRespostaNao.end());
        if (filaRespostaNao.empty())  resolverEfeitos(servidor);
        else if (eraOPrimeiro)        perguntarAoPrimeiro(servidor);
        } else {

            // Algum oponente do autor ainda tem carta? (mesmo teste do aplicarEfeito)
            bool alguemTemCarta = false;
            for (int id : ordemTurnos)
                if (id != ordemTurnos.front()) {
                    auto o = obterJogadorPorId(id);
                    if (o && o->jogador.getTamanhoMao() > 0) { alguemTemCarta = true; break; }
                }


            // O alvo que ia entregar a carta desconectou: o efeito continua, o autor escolhe outro alvo
            if ((aguardandoEscolhaCarta || aguardandoEscolhaTipoCarta) && idJogador == idJogadorDoador) {
                aguardandoEscolhaCarta = false;
                aguardandoEscolhaTipoCarta = false;
                idJogadorDoador = -1;
                if (alguemTemCarta) {
                    aguardandoEscolhaOponente = true;
                    std::string contexto = "FAVOR";
                    if (tipoDoacao == tipoDoacao::combo3) contexto = "COMBO3";
                    if (auto autor = obterJogadorPorId(ordemTurnos.front()))
                        enviar(*autor, "ESCOLHER_ALVO " + contexto + "\n");
                    notificarTodosMesa(servidor);
                } else {  // ninguém sobrou com cartas: não há efeito possível
                    servidor.broadcast("FAVOR_SEM_EFEITO\n");
                    anunciarTurno(servidor);
                }
            }

            // O autor ainda não escolheu o alvo e ninguém sobrou com cartas: encerra em vez de travar
            else if (aguardandoEscolhaOponente && !alguemTemCarta) {
                aguardandoEscolhaOponente = false;
                servidor.broadcast("FAVOR_SEM_EFEITO\n");
                anunciarTurno(servidor);
            }

            // Se outra pessoa desconectou enquanto o alvo/carta estava sendo escolhido, mantemos o estado
            else if (aguardandoEscolhaOponente || aguardandoEscolhaCarta || aguardandoEscolhaTipoCarta) {
                notificarTodosMesa(servidor);
            }
            else {
                anunciarTurno(servidor);
            }
        }
}

// O jogo acaba quando resta no máximo um jogador vivo (ordemTurnos só tem vivos).
// Devolve true se acabou e anuncia o vencedor.
bool Partida::verificarFimDeJogo(ServidorTCP& servidor) {
    if (ordemTurnos.size() > 1) return false;

    andamento = false;
    if (!ordemTurnos.empty())
        servidor.broadcast("FIM_DE_JOGO VENCEDOR " + std::to_string(ordemTurnos.front()) + "\n");
    return true;
}

//caso um jogador desconecte, remove uma bomba do baralho para manter o número correto de bombas no jogo
void Partida::removerUmaBombaDoBaralho() {
    auto it = std::find_if(baralho.begin(), baralho.end(),
                           [](const std::unique_ptr<Carta>& c) { return c->getTipo() == TipoCarta::Bomba; });
    if (it != baralho.end()) baralho.erase(it);
}





// =====================================================================
// Utilidades e estado da mesa
// =====================================================================
// Retorna o ponteiro para o jogador com o id fornecido, ou nullptr se não encontrado.
std::shared_ptr<ClienteConectado> Partida::obterJogadorPorId(int idJogador) const {
    for (const auto& c : jogadores)
        if (c->jogador.getId() == idJogador) return c;
    return nullptr;
}

//envia uma mensagem para um cliente específico, se o socket estiver válido
void Partida::enviar(const ClienteConectado& cliente, const std::string& msg) {
    if (cliente.socket >= 0) ServidorTCP::enviarTudo(cliente.socket, msg);
}

//manda uma mensagem para todos os jogadores conectados, mostrando o estado da mesa para cada um
void Partida::notificarTodosMesa(ServidorTCP&) {
    for (const auto& c : jogadores) enviar(*c, obterEstadoMesa(*c));
}


//monta a string que representa o estado da mesa para um jogador específico, incluindo baralho, descarte, vez, oponentes, mão do jogador e histórico de descarte
// Segmentos separados por '|'. O ÚLTIMO (histórico do descarte) o cliente só mostra sob demanda.
std::string Partida::obterEstadoMesa(const ClienteConectado& cliente) const {
    std::ostringstream oss;
    oss << "MESA_ESTADO ";

    // 1. Baralho e Pilha de Descarte
    oss << "Baralho: " << baralho.size() << " cartas | ";
    oss << "Descarte: " << pilhaDescarte.size() << " cartas | ";

    oss << "Vez de: ";
    if (ordemTurnos.empty()) {
        oss << "-";
    } else if (auto atual = obterJogadorPorId(ordemTurnos.front())) {
        oss << atual->jogador.getNome() << " (" << turnosPendentes << " turno(s) restante(s))";
    }
    oss << " | ";

    oss << "Oponentes: ";  //vivos
    for (const auto& c : jogadores) {
        if (c->jogador.getId() != cliente.jogador.getId() && c->jogador.estaVivo())
            oss << c->jogador.getNome() << " (" << c->jogador.getTamanhoMao() << " cartas)  ";
    }
    oss << "| ";

    // 3. Sua Mão
    oss << "Sua mao: ";
    const auto& mao = cliente.jogador.getMao();
    if (mao.empty()) oss << "VAZIA";
    for (size_t i = 0; i < mao.size(); ++i) oss << "[" << i << "] " << mao[i]->getNome() << "  ";
    oss << "| ";

    // 4. Cartas no Descarte (Histórico opcional)
    oss << "Historico_Descarte: ";
    if (pilhaDescarte.empty()) oss << "Vazio";
    for (const auto& c : pilhaDescarte) oss << c->getNome() << " ";
    oss << "\n";

    return oss.str();
}