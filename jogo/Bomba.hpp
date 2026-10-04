#include "Carta.hpp"
#include "ServidorTCP.hpp"
#include<algorithm>
#include<vector>
class ServidorTCP;
class Bomba : public Carta{
    public:
    Bomba(int id, const std::string& nome, const std::string& descricao, TipoCarta tipo)
        : Carta(id, nome, descricao, tipo) {}
    
    void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> jogadorAlvo) override{
      (void)servidor;     // Silencia o aviso do compilador
        (void)jogadorAlvo;

    }
};