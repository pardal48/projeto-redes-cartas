#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Carta.hpp"

// Dados de um jogador. A MÃO fica aqui (vector de unique_ptr<Carta>): o jogador é o dono das cartas.
class Jogador {
public:
    Jogador();
    ~Jogador();
    Jogador(const Jogador&) = delete;             // a mão tem unique_ptr: não é copiável
    Jogador& operator=(const Jogador&) = delete;

    void setId(int id);
    int getId() const;

    std::string getNome() const;
    void setNome(const std::string& nome);

    void setPronto(bool pronto);
    bool getPronto() const;

    bool estaVivo() const;
    void eliminar();

    std::vector<std::unique_ptr<Carta>>& getMao();
    std::size_t getTamanhoMao() const { return cartas.size(); }
    void adicionarCartaMao(std::unique_ptr<Carta> carta);
    std::unique_ptr<Carta> removerCartaMao(std::size_t indice);

    // Fim de partida: volta ao estado de lobby (não pronto, vivo, mão vazia).
    void reiniciarParaLobby();

private:
    int id = 0;
    std::string nome;
    bool pronto = false;
    bool vivo = true;
    std::vector<std::unique_ptr<Carta>> cartas;
};
