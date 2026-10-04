#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "Jogador.hpp"
//class Jogador; // Forward declaration of the Jogador class
class ServidorTCP;
class ClienteConectado;
enum class TipoCarta {
    // Cartas de Efeito
    Bomba,
    Desarme,
    Ataque,
    Pular,
    Nao,
    Embaralhar,
    Futuro,
    Favor,
    // Cartas Sem Efeito (Gatos)
    GatoMelancia,
    GatoTaco,
    GatoBarba,
    GatoBatata,
    GatoAranha
};
class Carta{

    private:
        int id;
        std::string nome;
        std::string descricao;
        TipoCarta tipo;


        public:
        Carta(int id, std::string nome, std::string descricao,TipoCarta tipo);
        ~Carta();
        //virtual void aplicarEfeito(Jogador& jogador)=0; // Carta é classe abstrata, um pouco diferente do java
        virtual void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> jogadorAlvo) = 0;
        void setId(int id);
        int getId() const ;
        std::string getNome() const ;
        std::string getDescricao() const;
        TipoCarta getTipo() const;
        //construtor pras subclasses de carta
        static std::unique_ptr<Carta> criar(int id, const std::string& nome, const std::string& desc, TipoCarta tipo);
};
