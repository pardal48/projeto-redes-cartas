#include"Carta.hpp"
#include"Jogador.hpp"
#include <iostream>
#include <string>
#include "CartaNormal.hpp"
#include "CartaPular.hpp"
#include "Bomba.hpp"
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




std::unique_ptr<Carta> Carta::criar(int id, const std::string& nome, const std::string& desc, TipoCarta tipo) {
    switch (tipo) {
        case TipoCarta::Pular:
            return std::make_unique<CartaPular>(id, nome, desc, tipo);
        //case TipoCarta::Ataque:
            //return std::make_unique<CartaAtaque>(id, nome, desc, tipo);
        case TipoCarta::Bomba:
            return std::make_unique<Bomba>(id, nome, desc, tipo);
        default:
            return std::make_unique<CartaNormal>(id, nome, desc, tipo);
    }
}
