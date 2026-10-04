#pragma once
#include <string>

// Tipos de carta do baralho.
enum class TipoCarta {
    Bomba,       // Exploding Kitten
    Desarme,     // Defuse
    Ataque,
    Pular,
    Favor,
    Embaralhar,
    Futuro,
    Nao,         // "Nem penses!"
    GatoAranha, GatoBarba, GatoBatata, GatoMelancia, GatoTaco
};

class Carta {
public:
    Carta(int id, std::string nome, std::string descricao, TipoCarta tipo);

    void setId(int novoId) { id = novoId; }
    int getId() const { return id; }
    const std::string& getNome() const { return nome; }
    const std::string& getDescricao() const { return descricao; }
    TipoCarta getTipo() const { return tipo; }

private:
    int id;
    std::string nome;
    std::string descricao;
    TipoCarta tipo;
};