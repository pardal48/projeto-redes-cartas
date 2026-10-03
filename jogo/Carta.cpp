#include"Carta.hpp"
#include"Jogador.hpp"
#include <iostream>
#include <string>


Carta::Carta(int id, std::string nome, std::string descricao,TipoCarta tipo) : id(id), nome(nome), descricao(descricao),tipo(tipo) {}
// mais rápido do que escrever dentro do construtor, pois evita a cópia de valores e 
//inicializa diretamente os membros da classe.

Carta::~Carta() {
    // Destrutor da classe Carta
}
void Carta::setId(int id) {
    this->id = id;
}
int Carta :: getId() const { return id; }
std::string Carta :: getNome() const { return nome; }
std::string Carta :: getDescricao() const { return descricao; }
TipoCarta Carta :: getTipo() const { return tipo; }
