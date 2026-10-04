#pragma once
// Interface.hpp - toda a saída/entrada de terminal do cliente.
// Todos os métodos que imprimem travam mtxTela: a thread de recepção e a thread principal
// escrevem na tela sem misturar linhas.
#include <mutex>
#include <string>

#include "ClienteTCP.hpp"

class Interface {
public:
    enum class ComandoLobby { Nenhum, AlternarPronto, Sair };
    enum class Leitura { Nada, Linha, Fim };  // resultado de ler_linha()

    bool TelaInicial();

    bool pedir_nome(std::string& nome);
    void mostrar_nome_invalido();

    void mostrar_lobby(const ClienteTCP::ListaLobby& jogadoresdoLobby, int meuId, int capacidade = 5);
    ComandoLobby ler_comando_lobby(int timeoutMs);

    // ---- partida ----
    // Lê uma linha digitada com timeout (poll em stdin). Nada = estourou o tempo.
    Leitura ler_linha(int timeoutMs, std::string& linha);
    // Traduz uma mensagem do servidor (ver README) para texto e imprime.
    void mostrar_evento(const std::string& linha, const ClienteTCP& cliente);
    void mostrar_ajuda_jogo();

    void mostrar_partida_iniciando();
    void mostrar_conexao_perdida();
    void mostrar_erro(const std::string& mensagem);

private:
    char lerTecla();
    std::mutex mtxTela;
};
