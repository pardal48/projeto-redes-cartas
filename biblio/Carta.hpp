#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "Jogador.hpp"
//class Jogador; // Forward declaration of the Jogador class

class Carta{

    private:
        int id;
        std::string nome;
        std::string descricao;


        public:
        Carta(int id, std::string nome, std::string descricao);

        virtual void aplicarEfeito(Jogador& jogador)=0; // Carta é classe abstrata, um pouco diferente do java
        void setId(int id);
};
