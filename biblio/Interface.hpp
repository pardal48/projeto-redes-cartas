#pragma once
#include <mutex>
#include <string>
#include <vector>
 
#include "ClienteTCP.hpp"  // só para o tipo ClienteTCP::ListaJogadores
 
// ---- MÚLTIPLOS SERVIDORES (desativado) ----
#if 0
#include "Conexao.hpp"  // ServidorInfo (ajuste o nome do arquivo)
#endif
 
// Tudo o que aparece na tela (e a leitura do que o usuário digita) fica aqui.
//
// A thread de recepção do cliente e a thread principal imprimem ao mesmo tempo,
// então toda saída passa pelo 'mtxTela' desta classe. A Interface é a dona
// desse mutex: quem quiser imprimir algo usa um método dela.

class Interface {
public:
    

        enum class ComandoLobby { Nenhum, AlternarPronto, Sair };
 
    // tela inicial 
    bool TelaInicial();  // false = usuário apertou ESC
 
    //nome
    // false = fim da entrada (Ctrl+D)
    bool pedir_nome(std::string& nome);
    void mostrar_nome_invalido();
 
    //lobby 
    // Não limpa a tela: cada atualização é impressa embaixo da anterior.
    //aqui é interessante implementar a limpeza de tela se tiverem tempo
    // Pode ser chamada pela thread de recepção.
    void mostrar_lobby(const ClienteTCP::ListaLobby& jogadoresdoLobby,
                       int meuId, int capacidade = 5);
 
    // Espera até 'timeoutMs' por uma linha digitada (P ou S + Enter).
    // Nenhum = nada digitado no prazo (ou comando desconhecido).
    ComandoLobby ler_comando_lobby(int timeoutMs);
 
    //mensagens gerais 
    void mostrar_partida_iniciando();
    void mostrar_conexao_perdida();
    void mostrar_erro(const std::string& mensagem);
 
    // ---- MÚLTIPLOS SERVIDORES (desativado) ----
#if 0
    void mostrar_servidores(const std::vector<ServidorInfo>& servidores);
    void escolher_servidor(const std::vector<ServidorInfo>& servidores, int& servidorEscolhido);
#endif
 
private:
    char lerTecla();
    std::mutex mtxTela;//mutex para proteger a saída na tela, evitando que múltiplas threads imprimam ao mesmo tempo




};