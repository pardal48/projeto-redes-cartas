#pragma once
// Conexao.hpp - informações de um servidor/lobby (nome, endereço, ocupação).
#include <string>
#include <utility>

struct ServidorInfo {
    std::string nome;
    std::string endereco = "localhost";
    int porta = 0;
    int jogadores = 0;
    int capacidade = 5;  // máximo de jogadores no lobby

    ServidorInfo() = default;
    ServidorInfo(std::string n, int p) : nome(std::move(n)), porta(p) {}

    bool estahCheio() const { return jogadores >= capacidade; }
};
