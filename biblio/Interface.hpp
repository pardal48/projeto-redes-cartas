#pragma once
#include <mutex>
#include <string>
#include <vector>

#include "ClienteTCP.hpp"

// Toda a entrada/saída do terminal do cliente.
// Saída: protegida por mtxTela (a thread de recepção também imprime).
// Entrada: lida com read()+poll() e buffer próprio, para o timeout não se perder com o buffer do std::cin.
class Interface {
public:
    enum class ComandoLobby { Nenhum, AlternarPronto, Sair };

    // ---- início ----
    bool TelaInicial();                       // false = o jogador pediu para sair (ESC)

    // ---- nome ----
    bool pedir_nome(std::string& nome);       // false = fim da entrada
    void mostrar_nome_invalido();

    // ---- lobby ----
    void mostrar_lobby(const ClienteTCP::ListaLobby& jogadores, int meuId, int capacidade = 5);
    ComandoLobby ler_comando_lobby(int timeoutMs);

    // ---- partida ----
    void mostrar_partida_iniciando();
    void mostrar_mesa(const std::string& estadoMesa, ModoJogo modo);
    void mostrar_descarte(const std::string& estadoMesa);
    void mostrar_prompt(ModoJogo modo);
    void mostrar_evento(const std::string& mensagem);
    std::string ler_comando_jogo(int timeoutMs = 200);  // "" = nada digitado; "SAIR" = fim da entrada

    // ---- mensagens gerais ----
    void mostrar_conexao_perdida();
    void mostrar_erro(const std::string& mensagem);

private:
    enum class Leitura { Linha, Timeout, Fim };

    Leitura lerLinha(std::string& linha, int timeoutMs);  // timeoutMs < 0: espera indefinidamente
    static char lerTecla();
    static std::vector<std::string> dividirSegmentos(const std::string& estadoMesa);
    void imprimirPrompt(ModoJogo modo);                   // mtxTela já travado

    std::mutex mtxTela;
    std::string bufferEntrada;
};