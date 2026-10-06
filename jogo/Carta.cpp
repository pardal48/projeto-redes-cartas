#include "Carta.hpp" // Inclui a definição da classe Carta

#include <utility>

// Construtor da classe Carta, inicializando seus atributos
Carta::Carta(int id, std::string nome, std::string descricao, TipoCarta tipo)
    : id(id), nome(std::move(nome)), descricao(std::move(descricao)), tipo(tipo) {}
