#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Carta.hpp"

class Jogador {
public:
    void setId(int novoId) { id = novoId; }
    int getId() const { return id; }

    const std::string& getNome() const { return nome; }
    void setNome(const std::string& novoNome) { nome = novoNome; }

    void setPronto(bool p) { pronto = p; }
    bool getPronto() const { return pronto; }

    bool estaVivo() const { return vivo; }
    void eliminar() { vivo = false; }

    // ---- mão ----
    const std::vector<std::unique_ptr<Carta>>& getMao() const { return cartas; }
    size_t getTamanhoMao() const { return cartas.size(); }
    void adicionarCartaMao(std::unique_ptr<Carta> carta);
    std::unique_ptr<Carta> removerCartaMao(size_t indice);  // nullptr se o índice for inválido

    // Volta ao estado de lobby depois de uma partida (mantém id e nome).
    void reiniciarParaLobby();

private:
    int id = -1;
    std::string nome;
    bool pronto = false;
    bool vivo = true;
    std::vector<std::unique_ptr<Carta>> cartas;
};