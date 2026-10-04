
#include "Carta.hpp"
class CartaPular : public Carta{
public:
    CartaPular(int id, std::string nome, std::string descricao, TipoCarta tipo)
        : Carta(id, std::move(nome), std::move(descricao), tipo) {}

    void aplicarEfeito(ServidorTCP&, std::shared_ptr<ClienteConectado>) override{
        
        // turno 

    }
};