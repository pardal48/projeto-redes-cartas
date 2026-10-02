#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "Carta.hpp"
#include <memory>
class Jogador { 
private:
    int id;
    std::string nome;
    std::vector<std::unique_ptr<Carta>> cartas; 
    // vetor de ponteiros únicos para objetos Carta, ou seja, cada carta é de propriedade exclusiva 
    //do jogador e será destruída automaticamente quando o jogador for destruído.


};
