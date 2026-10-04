#include "ClienteTCP.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <sstream>

// =====================================================================
// Conexão
// =====================================================================

int conectarTCP(const std::string& host, int porta, bool verboso) {
    addrinfo dicas{};
    dicas.ai_family = AF_INET;
    dicas.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    int erro = getaddrinfo(host.c_str(), std::to_string(porta).c_str(), &dicas, &res);
    if (erro != 0) {
        if (verboso) std::fprintf(stderr, "getaddrinfo(%s): %s\n", host.c_str(), gai_strerror(erro));
        return -1;
    }

    // Tenta cada endereço devolvido até um conectar.
    int sock = -1;
    for (addrinfo* a = res; a != nullptr; a = a->ai_next) {
        sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, a->ai_addr, a->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0 && verboso) perror("connect");
    return sock;
}

ClienteTCP::~ClienteTCP() { fechar(); }

bool ClienteTCP::conectar(const std::string& host, int porta) {
    fd = conectarTCP(host, porta);
    if (fd < 0) return false;

    {
        std::lock_guard<std::mutex> lock(mtx);
        ativo = true;
    }
    thRecepcao = std::thread(&ClienteTCP::receber, this);
    return true;
}

bool ClienteTCP::enviar(const std::string& linha) {
    std::lock_guard<std::mutex> lock(mtxEnvio);
    const std::string msg = linha + "\n";
    size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

void ClienteTCP::fechar() {
    if (fd < 0) return;
    shutdown(fd, SHUT_RDWR);                    // acorda o recv() da thread de recepção
    if (thRecepcao.joinable()) thRecepcao.join();
    close(fd);
    fd = -1;
    std::lock_guard<std::mutex> lock(mtx);
    ativo = false;
}

// =====================================================================
// Acesso ao estado (sempre sob mtx)
// =====================================================================

bool ClienteTCP::conectado() const { std::lock_guard<std::mutex> l(mtx); return ativo; }
bool ClienteTCP::iniciou() const   { std::lock_guard<std::mutex> l(mtx); return comecou; }
bool ClienteTCP::terminou() const  { std::lock_guard<std::mutex> l(mtx); return fimDeJogo; }
int ClienteTCP::meuId() const      { std::lock_guard<std::mutex> l(mtx); return id; }

bool ClienteTCP::esperarNome() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return nomeAceito || erroNome || !ativo; });
    const bool aceito = nomeAceito;
    erroNome = false;  // consome o erro para a próxima tentativa
    return aceito;
}

bool ClienteTCP::estouPronto() const {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& j : jogadoresDoLobby)
        if (j->getId() == id) return j->getPronto();
    return false;
}

ClienteTCP::LobbySnapshot ClienteTCP::snapshotLobby() const {
    std::lock_guard<std::mutex> lock(mtx);
    return {versaoLobby, jogadoresDoLobby};
}

ClienteTCP::MesaSnapshot ClienteTCP::snapshotMesa() const {
    std::lock_guard<std::mutex> lock(mtx);
    return {versaoMesa, estadoMesa};
}

ModoJogo ClienteTCP::modoAtual() const {
    std::lock_guard<std::mutex> lock(mtx);
    if (!vivo) return ModoJogo::Eliminado;
    if (aguardandoNao) return ModoJogo::Reagir;
    if (turnoAtual == id && !aguardandoMinhaCarta) return ModoJogo::MinhaVez;
    return ModoJogo::Aguardar;
}

void ClienteTCP::responderNao(bool jogarNao) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        aguardandoNao = false;  // se o servidor repetir a pergunta, a nova ordem a reativa
    }
    enviar(jogarNao ? "JOGAR_NAO" : "PASSO");
}

std::string ClienteTCP::nomeDe(int idJogador) const {
    for (const auto& j : jogadoresDoLobby)
        if (j->getId() == idJogador) return j->getNome();
    return "Jogador " + std::to_string(idJogador);
}

// =====================================================================
// Thread de recepção
// =====================================================================

void ClienteTCP::receber() {
    std::string entrada;
    char buffer[512];

    while (true) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;

        entrada.append(buffer, static_cast<size_t>(n));
        size_t pos;
        while ((pos = entrada.find('\n')) != std::string::npos) {
            std::string linha = entrada.substr(0, pos);
            entrada.erase(0, pos + 1);
            if (!linha.empty() && linha.back() == '\r') linha.pop_back();
            tratarLinha(linha);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        ativo = false;
    }
    cv.notify_all();  // acorda quem espera o nome
}

namespace {

// Traduz códigos de erro do servidor para algo legível.
std::string traduzirErro(const std::string& codigo) {
    if (codigo == "NAO_E_SEU_TURNO")                          return "Nao e a sua vez.";
    if (codigo == "INDICE_INVALIDO")                          return "Numero de carta invalido.";
    if (codigo == "CARTA_DE_REACAO_NAO_PODE_SER_JOGADA_ASSIM") return "Essa carta so e usada como reacao.";
    if (codigo == "CARTA_NAO_IMPLEMENTADA")                   return "Essa carta ainda nao foi implementada.";
    if (codigo == "AGUARDANDO_RESPOSTA_NAO")                  return "Aguarde: os outros jogadores estao reagindo.";
    if (codigo == "VOCE_NAO_TEM_A_CARTA_NAO")                 return "Voce nao tem a carta NAO.";
    if (codigo == "NAO_E_SUA_VEZ_DE_REAGIR")                  return "Nao e a sua vez de reagir.";
    if (codigo == "VOCE_ESTA_ELIMINADO")                      return "Voce foi eliminado.";
    if (codigo == "COMANDO_INVALIDO" || codigo == "COMANDO_INVALIDO_USE_NUMEROS")
                                                              return "Comando invalido.";
    return "Erro: " + codigo;
}

}  // namespace

// Interpreta uma linha do servidor. Atualiza o estado sob o lock e só depois
// chama os callbacks (fora do lock, para a interface poder consultar o estado).
void ClienteTCP::tratarLinha(const std::string& linha) {
    std::istringstream iss(linha);
    std::string cmd;
    iss >> cmd;

    bool lobbyMudou = false, mesaMudou = false, ehErro = false;
    std::string evento;

    {
        std::lock_guard<std::mutex> lock(mtx);

        if (cmd == "OK") {
            std::string arg;
            iss >> arg;
            if (arg != "NOME_ACEITO") {
                try { id = std::stoi(arg); } catch (...) {}
            }

        } else if (cmd == "ERRO") {
            std::string codigo;
            iss >> codigo;
            if (codigo == "NOME_INVALIDO")      erroNome = true;   // a Interface trata
            else if (codigo == "LOBBY_CHEIO")   ativo = false;
            else { evento = traduzirErro(codigo); ehErro = true; }

        } else if (cmd == "LOBBY") {
            // "LOBBY <id>:<nome>:<pronto>;..."
            ListaLobby novos;
            std::stringstream ss(linha.size() > 6 ? linha.substr(6) : "");
            std::string item;
            while (std::getline(ss, item, ';')) {
                size_t a = item.find(':'), b = item.rfind(':');
                if (a == std::string::npos || a == b) continue;  // item mal formado
                try {
                    auto j = std::make_shared<Jogador>();
                    j->setId(std::stoi(item.substr(0, a)));
                    j->setNome(item.substr(a + 1, b - a - 1));
                    j->setPronto(item.substr(b + 1) == "1");
                    novos.push_back(j);
                } catch (...) {}
            }
            jogadoresDoLobby = std::move(novos);
            for (const auto& j : jogadoresDoLobby)
                if (j->getId() == id) nomeAceito = true;  // o servidor só lista quem tem nome
            ++versaoLobby;
            lobbyMudou = true;

        } else if (cmd == "INICIAR") {
            comecou = true;

        } else if (cmd == "TURNO") {
            int t;
            if (iss >> t) turnoAtual = t;
            // Um novo turno encerra qualquer reação pendente.
            aguardandoNao = false;
            aguardandoMinhaCarta = false;

        } else if (cmd == "MESA_ESTADO") {
            estadoMesa = linha.size() > 12 ? linha.substr(12) : "";
            ++versaoMesa;
            mesaMudou = true;

        } else if (cmd == "JOGOU") {
            int autor = -1;
            std::string carta;
            iss >> autor >> carta;
            if (autor == id) {
                aguardandoMinhaCarta = true;  // o servidor confirmou a jogada: agora os outros reagem
                evento = "Voce jogou " + carta + ". Aguardando possiveis reacoes...";
            } else {
                evento = nomeDe(autor) + " jogou " + carta + ".";
            }

        } else if (cmd == "PERGUNTA_NAO") {
            int autor = -1;
            std::string carta;
            iss >> autor >> carta;
            aguardandoNao = true;
            evento = "[REACAO] " + nomeDe(autor) + " jogou " + carta + "! JOGAR_NAO para cancelar ou PASSO.";

        } else if (cmd == "JOGOU_NAO") {
            int autor = -1;
            iss >> autor;
            evento = (autor == id ? std::string("Voce") : nomeDe(autor)) + " jogou NAO!";

        } else if (cmd == "CANCELADO") {
            aguardandoNao = false;
            aguardandoMinhaCarta = false;
            evento = "A acao foi CANCELADA por uma carta NAO!";

        } else if (cmd == "EMBARALHOU") {
            evento = "O baralho foi embaralhado.";

        } else if (cmd == "FUTURO") {
            std::string resto;
            std::getline(iss, resto);
            evento = "As proximas 3 cartas (topo primeiro):" + resto;

        } else if (cmd == "COMPROU_VOCE") {
            std::string carta;
            iss >> carta;
            evento = "Voce comprou: " + carta;

        } else if (cmd == "COMPROU") {
            int autor = -1;
            iss >> autor;
            if (autor != id) evento = nomeDe(autor) + " comprou uma carta.";

        } else if (cmd == "EXPLOSAO") {
            int autor = -1;
            iss >> autor;
            evento = "BOOM! " + (autor == id ? std::string("Voce") : nomeDe(autor)) +
                     " comprou um Exploding Kitten!";

        } else if (cmd == "DEFUSOU") {
            int autor = -1;
            iss >> autor;
            evento = (autor == id ? std::string("Voce") : nomeDe(autor)) +
                     " usou DEFUSE e devolveu o gatinho ao baralho.";

        } else if (cmd == "MORREU") {
            int autor = -1;
            iss >> autor;
            if (autor == id) {
                vivo = false;
                aguardandoNao = false;
                aguardandoMinhaCarta = false;
                evento = "Voce explodiu e foi eliminado!";
            } else {
                evento = nomeDe(autor) + " explodiu e foi eliminado!";
            }

        } else if (cmd == "FIM_DE_JOGO") {
            std::string token;
            int vencedor = -1;
            iss >> token >> vencedor;
            fimDeJogo = true;
            evento = "FIM DE JOGO! Vencedor: " + nomeDe(vencedor) + (vencedor == id ? " (voce!)" : "");
        }
        // JOGO_INICIADO e comandos desconhecidos: ignorados.
    }

    cv.notify_all();
    if (!evento.empty() && aoEvento) aoEvento(evento, ehErro);
    if ((lobbyMudou || mesaMudou) && aoAtualizar) aoAtualizar();
}