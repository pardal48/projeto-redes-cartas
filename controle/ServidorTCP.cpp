#include "ServidorTCP.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <iostream>

// =====================================================================
// Rede básica
// =====================================================================

// send() pode enviar só parte da mensagem; repete até acabar.
bool ServidorTCP::enviarTudo(int fd, const std::string& msg) {
    if (fd < 0) return false;
    size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

ServidorTCP::ServidorTCP(int port, const std::string& nome) : porta(port), nomeServidor(nome) {
    servidorSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (servidorSocket == -1) {
        perror("erro ao criar socket");
        return;
    }

    int opt = 1;
    setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  // reinício rápido em testes

    sockaddr_in endereco{};
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(static_cast<uint16_t>(port));
    endereco.sin_addr.s_addr = INADDR_ANY;

    if (bind(servidorSocket, reinterpret_cast<sockaddr*>(&endereco), sizeof(endereco)) == -1) {
        perror("Erro ao fazer bind");
        close(servidorSocket);
        servidorSocket = -1;
        return;
    }

    // Se a porta pedida foi 0, o SO escolheu uma: descobre qual.
    sockaddr_in real{};
    socklen_t tamanho = sizeof(real);
    if (getsockname(servidorSocket, reinterpret_cast<sockaddr*>(&real), &tamanho) == 0)
        porta = ntohs(real.sin_port);

    if (listen(servidorSocket, SOMAXCONN) == -1) {
        perror("Erro ao escutar (listen)");
        close(servidorSocket);
        servidorSocket = -1;
        return;
    }

    info.nome = nomeServidor;
    info.porta = porta;
    rodando = true;
}

ServidorTCP::~ServidorTCP() {
    encerrar();
    if (servidorSocket != -1) close(servidorSocket);
}

// Loop de accept. O poll com timeout permite checar 'rodando' (Ctrl+C) a cada 500 ms.
void ServidorTCP::executar() {
    if (!ok()) return;
    std::cout << "Servidor ouvindo na porta " << porta << " (Ctrl+C para encerrar)\n";

    pollfd pfd{servidorSocket, POLLIN, 0};
    while (rodando) {
        int r = poll(&pfd, 1, 500);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (r == 0) continue;

        int fd = accept(servidorSocket, nullptr, nullptr);
        if (fd < 0) { perror("accept"); continue; }

        if (auto c = registrar(fd)) threads.emplace_back(&ServidorTCP::atenderCliente, this, c);
    }
    encerrar();
}

void ServidorTCP::encerrar() {
    rodando = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        // shutdown acorda os recv() bloqueados; quem faz close() é a thread de cada cliente.
        for (auto& c : clientes)
            if (c->socket >= 0) shutdown(c->socket, SHUT_RDWR);
    }
    // join FORA do lock: as threads precisam dele para se remover.
    for (auto& t : threads)
        if (t.joinable()) t.join();
    if (!threads.empty()) std::cout << "Servidor encerrado.\n";
    threads.clear();
}

// =====================================================================
// Ciclo de vida do cliente
// =====================================================================

// Aceita ou recusa a conexão (lobby cheio / partida em andamento).
std::shared_ptr<ClienteConectado> ServidorTCP::registrar(int fd) {
    std::lock_guard<std::mutex> lock(mtx);

    info.jogadores = static_cast<int>(clientes.size());
    if (info.estahCheio() || jogoIniciado) {
        enviarTudo(fd, "ERRO LOBBY_CHEIO\n");
        close(fd);
        return nullptr;
    }

    auto novo = std::make_shared<ClienteConectado>();
    novo->socket = fd;
    novo->jogador.setId(proximoId++);  // id próprio: não reaproveita o número do fd
    clientes.push_back(novo);

    enviarTudo(fd, "OK " + std::to_string(novo->jogador.getId()) + "\n");
    std::cout << "Cliente " << novo->jogador.getId() << " conectou (" << clientes.size() << " conexoes)\n";
    return novo;
}

// Thread de um cliente: monta linhas completas ('\n') e as processa sob o lock.
void ServidorTCP::atenderCliente(std::shared_ptr<ClienteConectado> client) {
    std::string entrada;
    char buffer[512];

    while (rodando) {
        ssize_t n = recv(client->socket, buffer, sizeof(buffer), 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;  // erro ou desconexão

        entrada.append(buffer, static_cast<size_t>(n));
        if (entrada.size() > 4096) {  // cliente mandando lixo sem '\n'
            std::cerr << "Mensagem muito grande do cliente " << client->jogador.getId() << "\n";
            break;
        }

        size_t pos;
        while ((pos = entrada.find('\n')) != std::string::npos) {
            std::string linha = entrada.substr(0, pos);
            entrada.erase(0, pos + 1);
            if (!linha.empty() && linha.back() == '\r') linha.pop_back();

            std::lock_guard<std::mutex> lock(mtx);
            processarLinha(*client, linha);
        }
    }
    removerCliente(client);
}

// Fecha o socket, avisa a partida e atualiza o lobby, tudo sob o mesmo lock.
void ServidorTCP::removerCliente(const std::shared_ptr<ClienteConectado>& client) {
    std::lock_guard<std::mutex> lock(mtx);
    const int id = client->jogador.getId();

    // Marca como desconectado ANTES de avisar a partida: ninguém envia para um fd fechado.
    close(client->socket);
    client->socket = -1;

    if (jogoIniciado && partidaAtual) {
        partidaAtual->removerJogador(id, *this);
        verificarFimDaPartida();
    }

    clientes.erase(std::remove(clientes.begin(), clientes.end(), client), clientes.end());
    std::cout << "Jogador " << id << " saiu (" << clientes.size() << " conexoes)\n";

    if (clientes.empty()) {
        jogoIniciado = false;
        partidaAtual.reset();
    }
    if (!jogoIniciado) broadcast(estadoComoTexto());  // durante a partida o lobby não interessa
}

// Se a partida acabou, volta ao estado de lobby (nomes preservados).
void ServidorTCP::verificarFimDaPartida() {
    if (!partidaAtual || partidaAtual->emAndamento()) return;

    std::cout << "Partida encerrada.\n";
    partidaAtual.reset();
    jogoIniciado = false;
    for (auto& c : clientes) c->jogador.reiniciarParaLobby();
}

// =====================================================================
// Lobby (mtx já travado)
// =====================================================================

void ServidorTCP::processarLinha(ClienteConectado& client, const std::string& linha) {
    // Partida em andamento: ela interpreta tudo.
    if (jogoIniciado && partidaAtual) {
        partidaAtual->processarComando(client, linha, *this);
        verificarFimDaPartida();
        return;
    }

    if (linha.rfind("NOME ", 0) == 0) {
        if (!client.jogador.getNome().empty()) {
            enviarTudo(client.socket, "ERRO JA_TEM_NOME\n");
            return;
        }
        const std::string nome = linha.substr(5);
        if (!nomeValido(nome) || nomeEmUso(nome)) {
            enviarTudo(client.socket, "ERRO NOME_INVALIDO\n");
            return;
        }
        client.jogador.setNome(nome);
        enviarTudo(client.socket, "OK NOME_ACEITO\n");

    } else if (linha == "PRONTO" || linha == "ESPERA") {
        if (client.jogador.getNome().empty()) {
            enviarTudo(client.socket, "ERRO ESCOLHA_UM_NOME\n");
            return;
        }
        client.jogador.setPronto(linha == "PRONTO");

    } else {
        // Sem isto o cliente ficaria esperando resposta para sempre.
        enviarTudo(client.socket, "ERRO COMANDO_DESCONHECIDO\n");
        return;
    }

    broadcast(estadoComoTexto());

    if (todosProntos()) {
        jogoIniciado = true;
        std::cout << "Todos prontos: iniciando partida...\n";
        broadcast("INICIAR\n");
        partidaAtual = std::make_unique<Partida>(clientes);
        partidaAtual->iniciar(*this);
        verificarFimDaPartida();  // cobre o caso de a partida nem ter começado
    }
}

void ServidorTCP::broadcast(const std::string& msg) {
    for (auto& c : clientes) enviarTudo(c->socket, msg);
}

bool ServidorTCP::nomeValido(const std::string& nome) const {
    if (nome.empty() || nome.length() > 12) return false;
    return std::all_of(nome.begin(), nome.end(),
                       [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; });
}

// Comparação sem diferenciar maiúsculas de minúsculas.
bool ServidorTCP::nomeEmUso(const std::string& nome) const {
    auto minusculo = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string alvo = minusculo(nome);
    for (const auto& c : clientes)
        if (minusculo(c->jogador.getNome()) == alvo) return true;
    return false;
}

bool ServidorTCP::todosProntos() const {
    if (clientes.size() < 2 || clientes.size() > static_cast<size_t>(info.capacidade)) return false;
    for (const auto& c : clientes)
        if (c->jogador.getNome().empty() || !c->jogador.getPronto()) return false;
    return true;
}

// Formato: LOBBY <id>:<nome>:<pronto>;<id>:<nome>:<pronto>;...
std::string ServidorTCP::estadoComoTexto() const {
    std::string s = "LOBBY ";
    for (const auto& c : clientes) {
        if (c->jogador.getNome().empty()) continue;  // ainda escolhendo nome
        s += std::to_string(c->jogador.getId()) + ":" + c->jogador.getNome() + ":" +
             (c->jogador.getPronto() ? "1" : "0") + ";";
    }
    return s + "\n";
}