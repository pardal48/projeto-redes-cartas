#pragma once
#include "Carta.hpp"
#include "Partida.hpp"
#include "ServidorTCP.hpp"
#include <utility>

class CartaFuturo : public Carta {
public:
    CartaFuturo(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}

    bool jogavelSozinha() const override { return true; }

    void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> ) override {
        // mostra em segredo as 3 primeiras cartas ao jogador da vez
        servidor.partidaAtiva().mostrarFuturo();
    }
};
