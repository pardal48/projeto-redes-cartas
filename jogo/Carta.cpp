#include"Carta.hpp"
#include"Jogador.hpp"
#include <iostream>
#include <string>


Carta::Carta(int id, std::string nome, std::string descricao) : id(id), nome(nome), descricao(descricao) {}
// mais rápido do que escrever dentro do construtor, pois evita a cópia de valores e 
//inicializa diretamente os membros da classe.

Carta::~Carta() {
    // Destrutor da classe Carta
}
void Carta::setId(int id) {
    this->id = id;
}

