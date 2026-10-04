# include "Interface.hpp"
#include <iostream>
#include <string>
#include<termios.h>
#include <unistd.h>
#include <poll.h>
#include <cctype>
#include <map>
#include <sstream>
#include <vector>
#include "TipoCarta.hpp"

/* essas duas funções são pra tentar implementar a visualização e escolha dentre vários servidores disponíveis,
mas eu sou um acéfalo e passei horas tentando e tomei no diff do c++, ignorem por enquanto */

/*
void Interface::mostrar_servidores(const std::vector<ServidorInfo>& servidores) {
    std::cout << "\n  === Servidores disponíveis ===\n";
    for (size_t i = 0; i < servidores.size(); ++i) {
        const ServidorInfo& servidor = servidores[i];
        std::cout << i + 1 << ". " << servidor.nome << ", Jogadores: " << servidor.jogadores
                  << "/" << servidor.capacidade << "\n";
    }
}
void Interface::escolher_servidor(const std::vector<ServidorInfo>& servidores, int& servidorEscolhido){
    int escolha;
    std::cout << "Escolha um servidor :";
    std::cin >> escolha;
    servidorEscolhido = escolha - 1;
    if(servidorEscolhido < 0 || static_cast<size_t>(servidorEscolhido) >= servidores.size()) {
        std::cout << "Escolha inválida. Tente novamente.\n";
        escolher_servidor(servidores, servidorEscolhido);
    }
    else if(servidores[servidorEscolhido].estahCheio()) {
        std::cout << "Servidor cheio. Escolha outro servidor.\n";
        escolher_servidor(servidores, servidorEscolhido);
    }
    else {
        std::cout << "Você escolheu o servidor: " << servidores[servidorEscolhido].nome << "\n";
    }
}
*/

char Interface::lerTecla() {
    termios antigo{};
    tcgetattr(STDIN_FILENO, &antigo);        // salva configuração atual

    termios novo = antigo;
    novo.c_lflag &= ~(ICANON | ECHO);        // desliga buffer de linha e eco
    tcsetattr(STDIN_FILENO, TCSANOW, &novo);

    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) c = 0;  // lê 1 caractere (EOF/erro => 0)

    tcsetattr(STDIN_FILENO, TCSANOW, &antigo); // restaura o terminal
    return c;
}

bool Interface::TelaInicial() {
    std::cout << "=== Bem-vindo ao Jogo de Cartas ===\n";
    std::cout << " aperte qualquer tecla para continuar\n";
    std:: cout << "ESC para sair\n";
    
    char tecla = lerTecla();
    if (tecla == 27) { // Código ASCII para ESC
        std::cout << "Saindo do jogo...\n";
        return false;
    }
    return true;
}


// ---------- nome ----------
 
bool Interface::pedir_nome(std::string& nome) {
    {
        std::lock_guard<std::mutex> lock(mtxTela);
        std::cout << "Escolha seu nome (letras e numeros, ate 12): " << std::flush;
    }
    // getline fora do lock: ele bloqueia esperando o usuário, e a thread de
    // recepção não pode ficar impedida de imprimir enquanto isso
    return static_cast<bool>(std::getline(std::cin, nome));
}
 
void Interface::mostrar_nome_invalido() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "Nome invalido ou ja em uso. Tente outro.\n";
}
 
// ---------- lobby ----------
 
void Interface::mostrar_lobby(const ClienteTCP::ListaLobby& jogadoresdoLobby,int meuId, int capacidade) {
    std::lock_guard<std::mutex> lock(mtxTela);
    // Mostra a lista de jogadores do lobby, indicando quem é o próprio jogador e se cada um está pronto ou não
    std::cout << "\n=== LOBBY (" << jogadoresdoLobby.size() << "/" << capacidade << ") ===\n";
    for (const auto& j : jogadoresdoLobby) {
        std::cout << "  " << (*j).getNome() << ((*j).getId() == meuId ? " (voce)" : "") << "  "
                  << ((*j).getPronto() ? "[PRONTO]" : "[aguardando]") << "\n";
    }
    std::cout << "Digite P + Enter para alternar pronto, S + Enter para sair.\n";
    std::cout.flush();// força a saída imediata, sem esperar o buffer encher
}
 
Interface::ComandoLobby Interface::ler_comando_lobby(int timeoutMs) {
    // Terminal normal: o poll só acusa dados quando o usuário já apertou Enter,
    // então o getline abaixo não trava.
    pollfd p{STDIN_FILENO, POLLIN, 0};
    if (poll(&p, 1, timeoutMs) <= 0) return ComandoLobby::Nenhum;
 
    std::string linha;
    if (!std::getline(std::cin, linha)) return ComandoLobby::Sair;  // Ctrl+D
 
    if (linha == "p" || linha == "P") return ComandoLobby::AlternarPronto;
    if (linha == "s" || linha == "S") return ComandoLobby::Sair;
    return ComandoLobby::Nenhum;
}
 
// ---------- mensagens gerais ----------
 
void Interface::mostrar_partida_iniciando() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nA partida vai comecar!\n";
}
 
void Interface::mostrar_conexao_perdida() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << "\nConexao com o servidor perdida.\n";
}
 
void Interface::mostrar_erro(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cerr << mensagem << "\n";
}


// ---------- partida ----------

Interface::Leitura Interface::ler_linha(int timeoutMs, std::string& linha) {
    // Mesmo esquema do lobby: o poll só acusa dados depois do Enter, então o getline não trava.
    pollfd p{STDIN_FILENO, POLLIN, 0};
    if (poll(&p, 1, timeoutMs) <= 0) return Leitura::Nada;
    if (!std::getline(std::cin, linha)) return Leitura::Fim;  // Ctrl+D
    return Leitura::Linha;
}

void Interface::mostrar_ajuda_jogo() {
    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout <<
        "\n=== COMANDOS ===\n"
        "  comprar (c)                 compra uma carta e encerra seu turno\n"
        "  jogar <ids> [alvo] [carta]  joga carta(s); <ids> = ids (entre colchetes na sua mao), separados por virgula\n"
        "      jogar 12               Atacar/Pular/Embaralhar/Ver o futuro (sem alvo)\n"
        "      jogar 12 Ana           Favor em Ana (alvo = nome ou id)\n"
        "      jogar 5,9 Ana          combo de 2 gatos iguais: rouba carta aleatoria de Ana\n"
        "      jogar 5,9,14 Ana DEFUSE combo de 3: pede uma carta (DEFUSE, NAO, PULAR, ATACAR, GATO1..GATO5...)\n"
        "  nao (n)                     joga um Nao durante a janela de reacao\n"
        "  dar <id>                    responde a um Favor entregando a carta <id>\n"
        "  pos <n>                     reinsere a bomba (0 = topo ... N = fundo)\n"
        "  mao (m) | mesa              mostra suas cartas | a mesa\n"
        "  sair (s)                    abandona a partida\n"
        "  ajuda (?)                   mostra esta lista\n";
    std::cout.flush();
}

namespace {

bool lerInteiroSeguro(const std::string& s, int& saida) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    saida = std::stoi(s);
    return true;
}

std::string nomeDoToken(const std::string& token) {
    TipoCarta t;
    return tokenParaTipo(token, t) ? tipoParaNomeLegivel(t) : token;
}

// "PULAR,GATO1" -> "Pular, Gato Aranha"
std::string nomesDeLista(const std::string& lista) {
    std::stringstream ss(lista);
    std::string item, saida;
    while (std::getline(ss, item, ',')) {
        if (!saida.empty()) saida += ", ";
        saida += nomeDoToken(item);
    }
    return saida;
}

std::string traduzirErro(const std::string& codigo) {
    static const struct { const char* c; const char* t; } kTabela[] = {
        {"NAO_E_SEU_TURNO", "Nao e a sua vez."},
        {"INDICE_INVALIDO", "Carta invalida: use o id de uma carta da sua mao (digite 'mao')."},
        {"ALVO_INVALIDO", "Alvo invalido (precisa ser outro jogador vivo)."},
        {"ALVO_SEM_CARTAS", "O alvo nao tem cartas."},
        {"COMBO_INVALIDO", "Combo invalido: use 2 ou 3 gatos IGUAIS."},
        {"CARTA_NAO_JOGAVEL", "Essa carta nao pode ser jogada assim."},
        {"NAO_FORA_DA_JANELA", "O Nao so pode ser jogado logo apos uma carta de efeito."},
        {"VOCE_NAO_TEM_A_CARTA_NAO", "Voce nao tem carta Nao."},
        {"AGUARDANDO_RESPOSTA_NAO", "Aguarde: ha uma carta aguardando reacoes (so 'nao' e permitido)."},
        {"FASE_INVALIDA", "Esse comando nao e valido agora."},
        {"ARGUMENTOS", "Argumentos incorretos (digite 'ajuda')."},
        {"CARTA_PEDIDA_INVALIDA", "Carta pedida invalida (ex.: DEFUSE, NAO, PULAR, GATO1..GATO5)."},
        {"POSICAO_INVALIDA", "Posicao invalida."},
        {"NAO_E_SUA_ESCOLHA", "Essa decisao nao e sua."},
        {"VOCE_ESTA_MORTO", "Voce explodiu: so pode assistir (mao, mesa, sair)."},
        {"JOGO_TERMINADO", "A partida ja terminou."},
        {"COMANDO_INVALIDO", "Comando invalido."},
        {"COMANDO_DESCONHECIDO", "Comando desconhecido."},
        {"JA_TEM_NOME", "Voce ja escolheu um nome."},
    };
    for (const auto& e : kTabela)
        if (codigo == e.c) return e.t;
    return "Erro: " + codigo;
}

}  // namespace

void Interface::mostrar_evento(const std::string& linha, const ClienteTCP& cli) {
    std::istringstream is(linha);
    std::vector<std::string> t;
    std::string w;
    while (is >> w) t.push_back(w);
    if (t.empty()) return;

    const int eu = cli.meuId();
    // Nome de quem agiu: "Voce" se for este cliente.
    auto quem = [&](const std::string& s, bool minusculo = false) -> std::string {
        int id;
        if (!lerInteiroSeguro(s, id)) return s;
        if (id == eu) return minusculo ? "voce" : "Voce";
        return cli.nomeDoJogador(id);
    };
    auto tem = [&](size_t n) { return t.size() >= n; };

    std::ostringstream out;
    const std::string& c = t[0];

    if (c == "CONTAGEM" && tem(2)) {
        out << "A partida comeca em " << t[1] << "...";
    } else if (c == "CONTAGEM_CANCELADA") {
        out << "Contagem interrompida (alguem mudou de status).";
    } else if (c == "INICIAR") {
        out << "\nA partida vai comecar!";
    } else if (c == "JOGO_INICIADO") {
        return;
    } else if (c == "ORDEM") {
        out << "Ordem dos turnos:";
        for (size_t i = 1; i < t.size(); ++i) out << (i > 1 ? " ->" : "") << " " << quem(t[i]);
    } else if (c == "MAO") {
        out << "Sua mao:";
        if (t.size() == 1) out << " (vazia)";
        for (size_t i = 1; i < t.size(); ++i) {
            size_t p = t[i].find(':');
            if (p == std::string::npos) continue;
            out << (i > 1 ? " |" : "") << " [" << t[i].substr(0, p) << "] " << nomeDoToken(t[i].substr(p + 1));
        }
    } else if (c == "STATUS") {
        // STATUS VEZ:<id> TURNOS:<n> JOGADORES_VIVOS:<n> BARALHO:<n> DESCARTE:<n> TOPO:<TOKEN|-> FASE:<f> JOGADORES:<id>:<vivo>:<n>;...
        std::map<std::string, std::string> kv;
        for (size_t i = 1; i < t.size(); ++i) {
            size_t p = t[i].find(':');
            if (p != std::string::npos) kv[t[i].substr(0, p)] = t[i].substr(p + 1);
        }
        int vez = 0;
        lerInteiroSeguro(kv["VEZ"], vez);
        out << "\n--- MESA --- baralho: " << kv["BARALHO"] << " | descarte: " << kv["DESCARTE"]
            << (kv["TOPO"] == "-" || kv["TOPO"].empty() ? "" : " (topo: " + nomeDoToken(kv["TOPO"]) + ")");
        std::stringstream ss(kv["JOGADORES"]);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t a = item.find(':'), b = item.rfind(':');
            if (a == std::string::npos || a == b) continue;
            const std::string idTxt = item.substr(0, a);
            int id = -1;
            lerInteiroSeguro(idTxt, id);
            const bool vivo = item.substr(a + 1, b - a - 1) == "1";
            out << "\n  " << (id == vez && vivo ? "> " : "  ") << quem(idTxt)
                << (vivo ? ("  cartas: " + item.substr(b + 1)) : "  [fora do jogo]");
            if (id == vez && vivo) out << "   <- vez (" << kv["TURNOS"] << " turno(s))";
        }
    } else if (c == "TURNO" && tem(2)) {
        int id = -1;
        lerInteiroSeguro(t[1], id);
        const std::string pend = tem(3) ? t[2] : "1";
        if (id == eu) out << "\n>>> SUA VEZ (" << pend << " turno(s)). Jogue cartas e/ou digite 'comprar'. <<<";
        else out << "\nVez de " << quem(t[1]) << " (" << pend << " turno(s)).";
    } else if (c == "JOGOU" && tem(5)) {
        out << quem(t[1]) << " jogou " << nomesDeLista(t[2]);
        if (t[3] != "0") out << " em " << quem(t[3], true);
        if (t[4] != "-") out << " pedindo " << nomeDoToken(t[4]);
    } else if (c == "PERGUNTA_NAO" && tem(4)) {
        out << "  (janela de reacao: " << t[3] << "s - digite 'nao' para anular)";
    } else if (c == "JOGOU_NAO" && tem(2)) {
        out << quem(t[1]) << " jogou NAO!";
    } else if (c == "APLICADO") {
        out << "  -> efeito aplicado.";
    } else if (c == "CANCELADO") {
        out << "  -> efeito CANCELADO.";
    } else if (c == "EMBARALHOU" && tem(2)) {
        out << quem(t[1]) << " embaralhou o baralho.";
    } else if (c == "FUTURO") {
        out << "Proximas cartas (topo primeiro):";
        if (t.size() == 1) out << " (baralho vazio)";
        for (size_t i = 1; i < t.size(); ++i) out << (i > 1 ? "," : "") << " " << nomeDoToken(t[i]);
    } else if (c == "FAVOR_PEDIDO" && tem(3)) {
        out << quem(t[1]) << " pediu um favor a " << quem(t[2], true) << ".";
    } else if (c == "DAR_CARTA" && tem(2)) {
        out << quem(t[1]) << " pediu um favor: escolha uma carta com 'dar <id>' ("
            << (tem(3) ? t[2] : "30") << "s, senao sera sorteada).";
    } else if (c == "FAVOR_CONCLUIDO" && tem(3)) {
        out << quem(t[2]) << " entregou uma carta a " << quem(t[1], true) << ".";
    } else if (c == "FAVOR_SEM_CARTAS") {
        out << "O favor nao teve efeito (alvo sem cartas).";
    } else if (c == "FAVOR_CANCELADO") {
        out << "O favor foi cancelado (um jogador saiu).";
    } else if (c == "ROUBO" && tem(4)) {
        out << quem(t[1]) << (t[3] == "1" ? " roubou uma carta de " : " tentou roubar de ") << quem(t[2], true)
            << (t[3] == "1" ? "." : ", sem sucesso.");
    } else if (c == "RECEBEU" && tem(3)) {
        out << "Voce recebeu: " << nomeDoToken(t[1])
            << (t[2] == "ROUBO" ? " (roubo)" : " (favor)");
    } else if (c == "PERDEU" && tem(2)) {
        out << "Voce perdeu: " << nomeDoToken(t[1]);
    } else if (c == "COMPROU" && tem(2)) {
        out << "Voce comprou: " << nomeDoToken(t[1]);
    } else if (c == "COMPRA" && tem(3)) {
        int id = -1;
        lerInteiroSeguro(t[1], id);
        if (id != eu) out << quem(t[1]) << " comprou uma carta (baralho: " << t[2] << ").";
        else return;
    } else if (c == "EXPLOSAO" && tem(2)) {
        out << "!!! " << quem(t[1]) << " comprou um GATINHO EXPLOSIVO !!!";
    } else if (c == "DEFUSOU" && tem(2)) {
        out << quem(t[1]) << " usou um Defuse!";
    } else if (c == "REINSERIR" && tem(2)) {
        out << "Reinsira a bomba: digite 'pos <n>' com n de 0 (topo) a " << t[1] << " (fundo).";
    } else if (c == "BOMBA_REINSERIDA" && tem(2)) {
        out << quem(t[1]) << " escondeu a bomba no baralho.";
    } else if (c == "MORREU" && tem(2)) {
        out << "*** " << quem(t[1]) << " EXPLODIU! ***";
    } else if (c == "SAIU" && tem(3)) {
        out << quem(t[1]) << " saiu da partida" << (t[2] == "MORTO" ? " (ja estava fora do jogo)." : ".");
    } else if (c == "AVISO") {
        out << "Aviso:" << (linha.size() > 5 ? linha.substr(5) : "");
    } else if (c == "FIM_DE_JOGO") {
        int id = -1;
        out << "\n=== FIM DE JOGO ===\n";
        if (tem(3) && t[1] == "VENCEDOR" && lerInteiroSeguro(t[2], id)) {
            if (id == eu) out << "VOCE VENCEU! Parabens!";
            else out << quem(t[2]) << " venceu a partida.";
        } else {
            out << "Nao houve vencedor.";
        }
        out << "\nVoltando ao lobby...";
    } else if (c == "ERRO" && tem(2)) {
        out << traduzirErro(t[1]);
    } else if (c == "ATE_LOGO") {
        return;  // resposta ao nosso SAIR; nada a mostrar
    } else {
        out << "[servidor] " << linha;  // mensagem desconhecida: mostra crua
    }

    std::lock_guard<std::mutex> lock(mtxTela);
    std::cout << out.str() << std::endl;
}
