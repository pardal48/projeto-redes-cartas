#pragma once
#include "Carta.hpp"
#include "Partida.hpp"
#include "ServidorTCP.hpp"
#include <utility>

class CartaAtaque : public Carta {
public:
    CartaAtaque(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}

    bool jogavelSozinha() const override { return true; }

    void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> ) override {
        // encerra o turno SEM comprar e o próximo faz 2 turnos (acumula se o atacante já estava sob ataque)
        servidor.partidaAtiva().atacarProximoJogador();
    }
};
