#pragma once
#include "Carta.hpp"
#include "Partida.hpp"
#include "ServidorTCP.hpp"
#include <utility>
class ServidorTCP;


class CartaPular : public Carta{
public:
    CartaPular(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}

    bool jogavelSozinha() const override { return true; }

    void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado>) override{
        // encerra UM turno do jogador da vez sem comprar (num Atacar, só 1 dos 2 turnos)
        servidor.partidaAtiva().encerrarTurnoSemComprar();
    }
};
