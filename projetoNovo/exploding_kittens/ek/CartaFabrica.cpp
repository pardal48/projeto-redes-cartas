// CartaFabrica.cpp - fábrica Carta::criar(). Fica separada de Carta.cpp porque as subclasses
// (CartaPular, Bomba...) dependem de Partida/ServidorTCP: o CLIENTE só precisa da classe base.
#include "Carta.hpp"
#include "CartaNormal.hpp"
#include "CartaPular.hpp"
#include "CartaAtaque.hpp"
#include "CartaEmbaralhar.hpp"
#include "CartaFuturo.hpp"
#include "CartaFavor.hpp"
#include "Bomba.hpp"

std::unique_ptr<Carta> Carta::criar(int id, const std::string& nome, const std::string& desc, TipoCarta tipo) {
    switch (tipo) {
        case TipoCarta::Pular:
            return std::make_unique<CartaPular>(id, nome, desc, tipo);
        case TipoCarta::Ataque:
            return std::make_unique<CartaAtaque>(id, nome, desc, tipo);
        case TipoCarta::Embaralhar:
            return std::make_unique<CartaEmbaralhar>(id, nome, desc, tipo);
        case TipoCarta::Futuro:
            return std::make_unique<CartaFuturo>(id, nome, desc, tipo);
        case TipoCarta::Favor:
            return std::make_unique<CartaFavor>(id, nome, desc, tipo);
        case TipoCarta::Bomba:
            return std::make_unique<Bomba>(id, nome, desc, tipo);
        default:  // Nao, Desarme e os gatos: sem efeito próprio (o Nao e os combos são tratados pela Partida)
            return std::make_unique<CartaNormal>(id, nome, desc, tipo);
    }
}
