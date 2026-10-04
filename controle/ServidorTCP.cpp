
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

//envia a mensagem completa, mesmo que o send() não consiga enviar tudo de uma vez, 
//ele vai continuar enviando até que toda a mensagem seja enviada
bool ServidorTCP :: enviarTudo(int fd, const std::string& msg) {
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
    }
    int opt = 1;
    setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // permite reiniciar a porta rapidamente, pra testes

    sockaddr_in servidor{};
    servidor.sin_family = AF_INET;//ipv4
    servidor.sin_port = htons(port);//porta
    servidor.sin_addr.s_addr = INADDR_ANY;//qualquer um conecta

    if (bind(servidorSocket, (sockaddr*)&servidor, sizeof(servidor)) == -1) {
        perror("Erro ao fazer bind");
        close(servidorSocket);
        servidorSocket = -1;
    }

    sockaddr_in enderecoReal{};
    socklen_t tamanho = sizeof(enderecoReal);
    if (getsockname(servidorSocket, (sockaddr*)&enderecoReal, &tamanho) == -1) {
        perror("Erro ao obter a porta real");
        close(servidorSocket);
        servidorSocket = -1;
    } else {
        porta = ntohs(enderecoReal.sin_port);//pega a porta gerada pelo SO, caso a porta passada seja 0

    }
    if (listen(servidorSocket, SOMAXCONN) == -1) {
        perror("Erro ao escutar (listen)");
        close(servidorSocket);
        servidorSocket = -1;
    }
    info.nome = nomeServidor;
    info.porta = getporta();

    rodando = true;
}

ServidorTCP::~ServidorTCP(){
    if (servidorSocket != -1) {
        close(servidorSocket);
    }
}

void ServidorTCP::parar() {rodando = false;}
/*
void ServidorTCP::escutar() {
    if (listen(servidorSocket, SOMAXCONN) == -1) {
        std::cerr << "erro listen" << std::endl;
    }
}

int ServidorTCP::aceitar(){
    sockaddr_in cliente{};
    socklen_t tamanho = sizeof(cliente);

    int clientID = accept(servidorSocket, (sockaddr*)&cliente, &tamanho);
    if (clientID == -1) {
        std:: cerr<<"erro aceitar"<<std ::endl;
    }

    return clientID;
}
*/
void ServidorTCP::executar() {
        std::cout << "Servidor ouvindo na porta " << porta << " (Ctrl+C para encerrar)\n";
    // o poll() é usado para permitir que o loop de aceitação seja interrompido pelo sinal de parada (Ctrl+C)
    // essencialmente, ele verifica se há conexões pendentes a cada 500 ms, 
    //permitindo que o servidor se encerre de forma controlada quando o sinal de parada é recebido
    pollfd pfd{servidorSocket, POLLIN, 0};
    while (rodando) {
        // poll só com timeout para checar 'rodando' a cada 500 ms
        int r = poll(&pfd, 1, 500);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("poll"); break;
        }
        if (r == 0) continue;
 
        int fd = accept(servidorSocket, nullptr, nullptr);
        if (fd < 0) { perror("accept"); continue; }
 
        auto c = registrar(fd);
        if (c) threads.emplace_back(&ServidorTCP::atenderCliente, this, c);// cria thread para atender o cliente
        // emplace_back é usado para construir o objeto thread diretamente no vetor, evitando cópias desnecessárias 
        //como ocorre com pushback
    }
    encerrar();// fecha todos os sockets e threads
}

//registra um novo cliente no lobby, atribuindo um ID único e adicionando-o à lista de clientes conectados.
std::shared_ptr<ClienteConectado> ServidorTCP::registrar(int fd) {
    //trava o mutex para garantir que o acesso à lista de clientes seja seguro em relação a múltiplas threads
    //impede acesso simultâneo à lista de clientes
 
    info.jogadores = static_cast<int>(clientes.size());
    if (info.estahCheio() || jogoIniciado) {
        enviarTudo(fd, "ERRO LOBBY_CHEIO\n");
        close(fd);
        return nullptr;
    }
 
    auto cliente_novo = std::make_shared<ClienteConectado>();
    cliente_novo->socket = fd;
    cliente_novo->jogador.setId(proximoId++);   // id próprio, não reaproveita como o fd
    clientes.push_back(cliente_novo);
    //formato da mensagem: "OK <id>\n", onde <id> é o ID único atribuído ao jogador recém-registrado
    enviarTudo(fd, "OK " + std::to_string(cliente_novo->jogador.getId()) + "\n");
    std::cout << "Cliente " << cliente_novo->jogador.getId() << " conectou ("<< clientes.size() << " conexões)\n";
    return cliente_novo;
}

void ServidorTCP::atenderCliente(std::shared_ptr<ClienteConectado> client) {
    std::string entrada;
    char buffer[512];//qualquer tamanho, mas não muito grande pra não travar a thread
    while(rodando){
        ssize_t n = recv((*client).socket, buffer, sizeof(buffer), 0);
        if (n <= 0 && errno == EINTR) {continue; }
        if (n <= 0) { break; }//erro ou cliente desconectou
        
        entrada.append(buffer, static_cast<size_t>(n));//adiciona os dados recebidos à string de entrada
        
        
        if (entrada.size() > 4096) { // limite de tamanho da mensagem
            std::cerr << "Mensagem muito grande do cliente " << (*client).jogador.getId() << "\n";
            break;
        }
        
        size_t posicao;
        while ((posicao = entrada.find('\n')) != std::string::npos) {
            std::string linha = entrada.substr(0, posicao);
            entrada.erase(0, posicao + 1);
            std:: lock_guard<std::mutex> lock(mtx);
            //trava o mutex para processar a linha de comando do cliente de forma segura em relação a múltiplas threads
            processarLinha(*client, linha);
            //processa a linha de comando do cliente, que pode ser um comando de jogo ou outro tipo de mensagem
        }
    }
    removerCliente(client);//remove o cliente da lista de clientes conectados quando ele se desconecta ou ocorre um erro
}


// Remoção e close(socket) juntos sob o lock: ninguém usa o fd depois de fechado.
void ServidorTCP::removerCliente(const std::shared_ptr<ClienteConectado>& client) {
    std::lock_guard<std::mutex> lock(mtx);
    //apaga o cliente da lista de clientes conectados
    // o remove joga o ponteiro para o final do vetor, e o erase remove ele de fato, liberando a memória
    clientes.erase(std::remove(clientes.begin(), clientes.end(), client), clientes.end());
    close((*client).socket);//fecha o socket do cliente para liberar recursos do sistema
    std::cout << "Jogador " << (*client).jogador.getId() << " saiu ("<< clientes.size() << " conexões)\n";
    //se o lobby estiver vazio, o jogo volta ao estado normal, permitindo que novos jogadores entrem
    if (clientes.empty()) jogoIniciado = false;  // lobby volta ao normal
    broadcast(estadoComoTexto());//envia a todos os clientes conectados o estado atual do lobby, incluindo informações sobre os jogadores presentes e se o jogo já começou
    cv.notify_all();//notifica todas as threads que estão esperando por uma condição específica, permitindo que elas continuem a execução
}

void ServidorTCP::encerrar() {
    rodando = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        // shutdown acorda os recv bloqueados; quem dá close é a thread de cada cliente
        for (auto& c : clientes) shutdown((*c).socket, SHUT_RDWR);
    }
    cv.notify_all();
 
    // join FORA do lock: as threads precisam dele para se remover
    for (auto& t : threads)
        if (t.joinable()) t.join();
    threads.clear();
    std::cout << "Servidor encerrado.\n";
}


// Lógica do lobby a partir daqui, processando comandos de jogo e mensagens dos clientes
void ServidorTCP::processarLinha(ClienteConectado& client, const std::string& linha) {
    // ---- MÚLTIPLOS SERVIDORES (desativado) ----
    // INFO serve para o menu de servidores do cliente (consultarServidor).
    // Deve ficar antes do 'if (jogoIniciado)' para funcionar em qualquer fase.

#if 0// o if 0 são parte dos múltiplos de servidores, ainda não testado
    if (linha == "INFO") {
        info.jogadores = static_cast<int>(jogadoresNoLobby());
        enviarTudo(client.socket, "INFO " + std::to_string(info.jogadores) + " " +std::to_string(info.capacidade) + " " + info.nome + "\n");
        return;
    }
#endif
 
    if (linha.rfind("NOME ", 0) == 0) {
        if (!client.jogador.getNome().empty()) { 
            enviarTudo(client.socket, "ERRO JA_TEM_NOME\n"); 
            return; 
        }
        
        // Se a linha for exatamente "NOME ", o substr(5) retorna "" (string vazia) com segurança
        std::string nome = linha.substr(5);
        
        // Valida no lado do servidor
        if (!nomeValido(nome)) { 
            enviarTudo(client.socket, "ERRO NOME_INVALIDO\n"); 
            return; 
        }
        
        // Tudo certo: define o nome e AVISA o cliente
        client.jogador.setNome(nome);
        enviarTudo(client.socket, "OK NOME_ACEITO\n");

    } else if (linha == "PRONTO" && !client.jogador.getNome().empty()) {
        client.jogador.setPronto(true);
    } else if (linha == "ESPERA") {
        client.jogador.setPronto(false);
    } else {
        // Fallback fundamental: Se vier lixo ("NOME" sem espaço, comando nulo, etc)
        // O servidor devolve erro para não deixar o cliente esperando para sempre.
        enviarTudo(client.socket, "ERRO COMANDO_DESCONHECIDO\n");
        return;  
    }

    broadcast(estadoComoTexto());
 
    if (todosProntos()) {
        jogoIniciado = true;
        std::cout << "Todos prontos: iniciando partida...\n";
        broadcast("INICIAR\n");
    }
}

// Ponto de entrada para as regras do jogo (turno, pilha de efeitos, reação...).
// Aqui dentro o mtx já está travado.
void ServidorTCP::processarComandoJogo(ClienteConectado&, const std::string&) {
    // TODO: máquina de estados do jogo
}


size_t ServidorTCP::jogadoresVivosCount() const {
    size_t contador = 0;
    for (const auto& client : clientes) {
        if (client->jogador.estaVivo()) {
            contador++;
        }
    }
    return contador;
}

void ServidorTCP::verificarFimDeJogo() {
    //esse mutex depende, se grantirmos que já vai estar lockado chegando aqui não precisa
    //std::lock_guard<std::mutex> lock(mtx);
    
    // Se sobrar apenas 1 jogador vivo com a partida em andamento
    if (jogoIniciado && jogadoresVivosCount() <= 1) {
        std::shared_ptr<ClienteConectado> vencedor = nullptr;
        for (const auto& client : clientes) {
            if ((*client).jogador.estaVivo()) {
                vencedor = client;
                break;
            }
        }

        if (vencedor) {
            std::cout << "Partida encerrada! Vencedor: " << (*vencedor).jogador.getNome() << "\n";
            broadcast("FIM_DE_JOGO " + std::to_string((*vencedor).jogador.getId()) + "\n");
        } else {
            broadcast("FIM_DE_JOGO EMPATE\n");
        }
        
        jogoIniciado = false; // Retorna para o lobby
    }
}

void ServidorTCP::broadcast(const std::string& msg) {//manda informação para todos os clientes conectados, como o estado do lobby ou mensagens de jogo
    for (auto& client : clientes) enviarTudo((*client).socket, msg);
}
bool ServidorTCP::nomeValido(const std::string& nome) const {
    // 1. Rejeita se for vazio (o cliente enviou só "NOME ")
    // 2. Rejeita se tiver mais de 12 caracteres
    if (nome.empty() || nome.length() > 12) {
        return false;
    }

    // 3. Garante que todos os caracteres são letras ou números
    for (char c : nome) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

bool ServidorTCP::todosProntos() const {
    if (clientes.size() < 2 || clientes.size() > static_cast<size_t>(info.capacidade)) return false;
    for (auto& cliente : clientes)
        if ((*cliente).jogador.getNome().empty() || !(*cliente).jogador.getPronto()) return false;
    return true;
}

size_t ServidorTCP::jogadoresNoLobby() const {
    size_t n = 0;
    for (auto& client : clientes)
        if (!(*client).jogador.getNome().empty()) n++;// só conta quem já escolheu nome, ou seja, quem está no lobby de fato
    return n;
}
//modo que a mensagem fica:
//LOBBY <id1>:<nome1>:<pronto1>;<id2>:<nome2>:<pronto2>;...
std::string ServidorTCP::estadoComoTexto() const {//estado atual do lobby como uma string, incluindo informações sobre os jogadores presentes
    std::string s = "LOBBY ";
    bool pronto;
    for (auto& client : clientes) {
        if ((*client).jogador.getNome().empty()) continue;  // ainda escolhendo nome
        pronto = (*client).jogador.getPronto();
        //adiciona as informações de cada jogador no lobby à string de estado, incluindo ID, nome e se está pronto ou não
        s += std::to_string((*client).jogador.getId()) + ":" + (*client).jogador.getNome() + ":";
        if(pronto){
            s += "1";
        }else{
            s += "0";
        }
        s += ";";
    }
    return s + "\n";
}

/*
void ServidorTCP::enviar(int clientID, const std::string& mensagem){
    send(clientID, mensagem.data(), mensagem.size(), 0);
}
//em receber podemos por outros tipos pra serem recebidos, isso é só um place holder
std::string ServidorTCP::receber(int clienteID){
    char buffer[1024];

    int bytes = recv(clienteID,buffer,sizeof(buffer),0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(buffer,bytes);

}
*/
int ServidorTCP::getporta() const {
    return porta;
}

