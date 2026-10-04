#pragma once
#include "Carta.hpp"
#include "Partida.hpp"
#include "ServidorTCP.hpp"
#include <utility>

class CartaFavor : public Carta {
public:
    CartaFavor(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}

    bool jogavelSozinha() const override { return true; }
    bool precisaAlvo() const override { return true; }

    void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> jogadorAlvo) override {
        // o alvo escolhe e entrega 1 carta ao jogador da vez
        servidor.partidaAtiva().iniciarFavor(jogadorAlvo);
    }
};
