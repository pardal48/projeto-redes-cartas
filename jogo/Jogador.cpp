#include "Jogador.hpp"
#include "Carta.hpp"

Jogador::Jogador() {
    // Constructor implementation
}

Jogador::~Jogador() {
    // Destructor implementation
}

void Jogador::setId(int id) { this->id = id; }
int Jogador::getId() const { return id; }

std::string Jogador::getNome() const { return nome; }
void Jogador::setNome(const std::string& nome) { this->nome = nome; }

void Jogador::setPronto(bool pronto) { this->pronto = pronto; }
bool Jogador::getPronto() const { return pronto; }