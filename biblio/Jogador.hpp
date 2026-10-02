#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <memory>
class Carta; // Forward declaration of the Carta class
class Jogador { 
private:
    int id;
    std::string nome;
    std::vector<std::unique_ptr<Carta>> cartas; 
    // vetor de ponteiros únicos para objetos Carta, ou seja, cada carta é de propriedade exclusiva 
    //do jogador e será destruída automaticamente quando o jogador for destruído.
public:
    Jogador();
    ~Jogador();

};
