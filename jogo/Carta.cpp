#include "Carta.hpp"

#include <utility>

Carta::Carta(int id, std::string nome, std::string descricao, TipoCarta tipo)
    : id(id), nome(std::move(nome)), descricao(std::move(descricao)), tipo(tipo) {}