#include "ClienteTCP.hpp"
 
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
 
#include <cerrno>
#include <cstdio>
#include <sstream>



 

int conectarTCP(const std::string& host, int porta, bool verboso){

    addrinfo dicas{};//estrutura que contém informações sobre o tipo de conexão desejada (IPv4, TCP, etc.)
    dicas.ai_family = AF_INET;
    dicas.ai_socktype = SOCK_STREAM;
 
    addrinfo* res = nullptr;//ponteiro para armazenar a lista de endereços retornada pelo getaddrinfo()
    int erro = getaddrinfo(host.c_str(), std::to_string(porta).c_str(), &dicas, &res);
    if (erro != 0) {
        if (verboso)//se verboso for true, imprime uma mensagem de erro detalhada
            std::fprintf(stderr, "getaddrinfo(%s): %s\n", host.c_str(), gai_strerror(erro));
        return -1;
    }
 
    int fd = -1;
    for (addrinfo* possivel_endereco = res; possivel_endereco != nullptr; possivel_endereco = (*possivel_endereco).ai_next) {  
        // tenta cada endereço
        fd = socket((*possivel_endereco).ai_family, (*possivel_endereco).ai_socktype, (*possivel_endereco).ai_protocol);
        if (fd < 0) continue;
        //se a conexão for bem-sucedida, o loop é interrompido e o socket é retornado; caso contrário, 
        //o socket é fechado e o próximo endereço é tentado, isso serve pra analisar os multiplos servidores
        if (connect(fd, (*possivel_endereco).ai_addr, (*possivel_endereco).ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
 
    if (fd < 0 && verboso) perror("connect");
    return fd;


}

// ---- MÚLTIPLOS SERVIDORES (desativado) ----
#if 0
// Lê até '\n' (byte a byte: é só uma linha curta). false = timeout ou conexão fechada.
static bool lerLinha(int fd, std::string& out) {
    char ch;
    out.clear();
    while (true) {
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1) return false;
        if (ch == '\n') return true;
        if (ch != '\r') out += ch;
    }
}
 // pega as informações do servidor (nome, capacidade, jogadores) sem entrar no lobby.
bool consultarServidor(ServidorInfo& info) {
    int fd = conectarTCP(info.endereco, info.porta, false);
    if (fd < 0) return false;
 
    timeval tv{2, 0};  // não trava o menu se o servidor não responder
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
 
    bool ok = false;// indica se a consulta foi bem-sucedida
    std::string linha;
    if (lerLinha(fd, linha)) {
        if (linha == "ERRO LOBBY_CHEIO") {
            info.jogadores = info.capacidade;  // estahCheio() passa a ser true
            ok = true;
        } else if (linha.rfind("OK", 0) == 0) {//se a linha recebida começa com "OK", significa que o servidor está respondendo corretamente à consulta
            send(fd, "INFO\n", 5, MSG_NOSIGNAL);
            // o servidor pode mandar um LOBBY antes; procura a linha INFO
            while (lerLinha(fd, linha)) {
                int j = 0, cap = 0, lidos = 0;
                if (std::sscanf(linha.c_str(), "INFO %d %d %n", &j, &cap, &lidos) >= 2) {
                    info.jogadores = j;
                    info.capacidade = cap;
                    if (lidos > 0 && static_cast<size_t>(lidos) < linha.size())
                        info.nome = linha.substr(static_cast<size_t>(lidos));
                    ok = true;
                    break;
                }
            }
        }
    }
    close(fd);
    return ok;
}
#endif

// ---------- ClienteTCP ----------
 
ClienteTCP::~ClienteTCP() { fechar(); }


bool ClienteTCP::conectar(const std::string& host, int porta) {
    fd = conectarTCP(host, porta);
    if (fd < 0) return false;
 
    ativo = true;// marca o socket como ativo, indicando que a conexão foi estabelecida com sucesso
    // inicia a thread de recepção, que vai ler o socket e atualizar o estado do lobby
    thRecepcao = std::thread(&ClienteTCP::receber, this);
    return true;
}

bool ClienteTCP::enviar(const std::string& linha) {
    std::lock_guard<std::mutex> lock(mtxEnvio);
    std::string msg = linha + "\n";
    size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = send(fd, msg.data() + total, msg.size() - total, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}


bool ClienteTCP::esperarNome() {// espera até que o servidor aceite ou rejeite o nome do jogador
    std::unique_lock<std::mutex> lock(mtx);
    // espera até que o servidor aceite ou rejeite o nome do jogador, ou até que a conexão seja fechada
    cv.wait(lock, [this] { return nomeAceito || erroNome || !ativo; });
    bool aceito = nomeAceito;
    erroNome = false;  // consome o erro para a próxima tentativa
    return aceito;
}
 
void ClienteTCP::fechar() {
    if (fd < 0) return;
    shutdown(fd, SHUT_RDWR);  // acorda o recv da thread de recepção
    if (thRecepcao.joinable()) thRecepcao.join();//verifica se a thread de recepção está em execução e, 
    //se estiver, aguarda sua conclusão antes de prosseguir
    close(fd);
    fd = -1;
    ativo = false;
}
 
ClienteTCP::ListaLobby ClienteTCP::lobby() const {// retorna uma cópia da lista de jogadores do lobby, protegida por mutex
    std::lock_guard<std::mutex> lock(mtx);
    return jogadoresDoLobby;  // cópia: quem chama não depende do lock
}
 
int ClienteTCP::meuId() const {// retorna o id do jogador no lobby, protegido por mutex
    std::lock_guard<std::mutex> lock(mtx);
    return id;
}

bool ClienteTCP::estouPronto() const {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& j : jogadoresDoLobby)
        if ((*j).getId() == id) return (*j).getPronto();
    return false;
}

//thred de recepção, a ideia é que ela vai ficar rodando em paralelo com a thread principal, 
//lendo o socket e atualizando o estado do lobby

void ClienteTCP::receber() {
    std::string entrada;
    char buffer[512];
 
    while (true) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        //se o recv retornar -1 e errno for EINTR, significa que a chamada foi interrompida por um sinal, 
        //então o loop continua para tentar novamente
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        
        //adiciona os dados recebidos à string de entrada
        entrada.append(buffer, static_cast<size_t>(n));
        size_t pos;//enquanto houver uma linha completa na string de entrada (terminada por '\n'), processa a linha
        while ((pos = entrada.find('\n')) != std::string::npos) {
            std::string linha = entrada.substr(0, pos);
            entrada.erase(0, pos + 1);
            if (!linha.empty() && linha.back() == '\r') linha.pop_back();
            tratarLinha(linha);//
        }
    }
 
    {
        std::lock_guard<std::mutex> lock(mtx);
        ativo = false;
    }
    cv.notify_all();//acorda quem está esperando a resposta do servidor (nome aceito ou não);
    if (aoAtualizar) aoAtualizar();//acorda a thread principal para atualizar a interface do usuário, se o callback estiver definido
}

//processa uma linha recebida do servidor (LOBBY, INICIAR, ERRO, etc.)
void ClienteTCP::tratarLinha(const std::string& linha) {
    bool mudou = false;//indica se houve alguma mudança no estado do lobby, para atualizar a interface do usuário
    {
        std::lock_guard<std::mutex> lock(mtx);
        //se a linha recebida começa com "OK ", significa que o servidor aceitou o nome do jogador e retornou o id do jogador
        if (linha.rfind("OK ", 0) == 0) {
            //extrai o id do jogador a partir da linha recebida, que está no formato "OK <id>"
            try { id = std::stoi(linha.substr(3)); } catch (...) {}
        } else if (linha == "ERRO NOME_INVALIDO") {
            erroNome = true;
        } else if (linha == "ERRO LOBBY_CHEIO") {
            ativo = false;
            //se o lobby estiver cheio, o cliente é desconectado e a thread principal é notificada
        } else if (linha.rfind("LOBBY", 0) == 0) {
            ListaLobby novos;
            //se o tamanho da linha for maior que 6, significa que há informações sobre os jogadores no lobby,
            //então a substring a partir do índice 6 é passada para um stringstream para ser processada
            //std::stringstream ss(linha.size() > 6 ? linha.substr(6) : "");
            std::stringstream ss;
            if (linha.size() > 6) {
                ss << linha.substr(6);//pula LOBBY e o espaço, deixando apenas a parte com as informações dos jogadores
            }
            std::string item;//cada item representa um jogador no formato "<id>:<nome>:<pronto>"
            //o loop while lê cada item separado por ';' e extrai as informações do jogador, 
            //criando um objeto Jogador para cada um
            while (std::getline(ss, item, ';')) {
                size_t a = item.find(':');//procura o primeiro ':' na string item, que separa o id do nome do jogador
                size_t b = item.rfind(':');//procura o último ':' na string item, que separa o nome do jogador do estado de pronto
               
               //se não houver ':' ou se o primeiro ':' for igual ao último ':', 
               //significa que o item está mal formatado, então o loop continua para o próximo item
                if (a == std::string::npos || a == b) continue;
                try {
                    int idJogador = std::stoi(item.substr(0, a));  // pode lançar: antes de criar o objeto
                    auto j = std::make_shared<Jogador>();
                    j->setId(idJogador);
                    //b-a-1 é o tamanho do nome do jogador, que é a substring entre os dois ':'
                    j->setNome(item.substr(a + 1, b - a - 1));
                    j->setPronto(item.substr(b + 1) == "1");
                    novos.push_back(j);
                } catch (...) {}
            }
            //atualiza a lista de jogadores do lobby com os novos objetos Jogador criados a partir das informações recebidas do servidor
            jogadoresDoLobby = std::move(novos);
            for (auto& j : jogadoresDoLobby)
                if (j->getId() == id) nomeAceito = true;  // o servidor só lista quem tem nome
            mudou = true;//indica que houve uma mudança no estado do lobby, para atualizar a interface do usuário
        } else if (linha == "INICIAR") {
            
            comecou = true;  // o laço do lobby percebe em até 200 ms (sem redesenhar a lista)
        }else if (linha.rfind("MESA_ESTADO ", 0) == 0) {
            
            estadoMesaAtual = linha.substr(12);
            mesaAtualizada = true; // Avisa a thread principal que a mesa chegou
            
            mudou = true;
        } else if (linha.rfind("TURNO ", 0) == 0) {
            try { turnoAtual = std::stoi(linha.substr(6)); } catch (...) {}
            std::cout << "\n>>> [SISTEMA]: " << linha << " <<<\n";
        } else if (linha.rfind("JOGOU", 0) == 0 || linha.rfind("COMPROU", 0) == 0 || linha.rfind("EXPLOSAO", 0) == 0) {
            std::cout << "\n>>> [SISTEMA]: " << linha << " <<<\n";
        }else if (linha.rfind("ERRO", 0) == 0) {
            std::cout << "\n>>> [SISTEMA]: " << linha << " <<<\n";
            // Se for um erro no meio do jogo, devolve o turno para tentar de novo
            if (linha != "ERRO NOME_INVALIDO" && linha != "ERRO LOBBY_CHEIO") {
                turnoAtual = id; 
            }
        }
    }
    cv.notify_all();//acorda quem está esperando a resposta do servidor (nome aceito ou não)
    if (mudou && aoAtualizar) aoAtualizar();  // fora do lock
}

std::string ClienteTCP::obterEstadoMesaLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    return estadoMesaAtual;
}

void ClienteTCP::esperarMesa() {
    std::unique_lock<std::mutex> lock(mtx);
    // Espera até 3 segundos pela mesa. Se o tempo estourar, acorda sozinho.
    bool chegou = cv.wait_for(lock, std::chrono::seconds(3), [this] { 
        return mesaAtualizada || !ativo; 
    });

    if (chegou && ativo) {
        mesaAtualizada = false;  // Consome o aviso
    } else if (!chegou) {
        std::cerr << "\n[Aviso] Demora na resposta do servidor. A mesa pode estar desatualizada.\n";
    }
}

int ClienteTCP::obterTurnoAtual() {
    std::lock_guard<std::mutex> lock(mtx);
    return turnoAtual;
}
 
void ClienteTCP::setTurnoAtual(int t) {
    std::lock_guard<std::mutex> lock(mtx);
    turnoAtual = t;
}