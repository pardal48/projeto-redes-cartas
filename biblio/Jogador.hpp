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
    bool pronto = false; // Indicates whether the player is ready
    // vetor de ponteiros únicos para objetos Carta, ou seja, cada carta é de propriedade exclusiva 
    //do jogador e será destruída automaticamente quando o jogador for destruído.
public:
    Jogador();
    ~Jogador();
    void setId(int id);
    int getId() const;// const indica que o método não modifica o estado do objeto, ou seja, não altera nenhum membro da classe.
    std::string getNome() const;
    void setNome(const std::string& nome);
    void setPronto(bool pronto);
    bool getPronto() const;
};
