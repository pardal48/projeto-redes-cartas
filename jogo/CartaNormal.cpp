
#include "Carta.hpp"

class CartaNormal : public Carta {
public:
    CartaNormal(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}
    
    void aplicarEfeito(ServidorTCP&, std::shared_ptr<ClienteConectado>) override {
        // Vazio: o efeito é passivo ou resolvido externamente
    }
};