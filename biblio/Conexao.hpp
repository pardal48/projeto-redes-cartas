#pragma once
#include <string>
#include <utility>

#include "Jogador.hpp"

// Informações públicas de um servidor (nome, porta, ocupação).
struct ServidorInfo {
    std::string nome;
    std::string endereco = "localhost";
    int porta = 0;
    int jogadores = 0;
    int capacidade = 5;

    ServidorInfo() = default;
    ServidorInfo(std::string n, int p) : nome(std::move(n)), porta(p) {}

    bool estahCheio() const { return jogadores >= capacidade; }
};

// Um cliente conectado ao servidor: o socket e o jogador associado.
// socket == -1 significa "já desconectado" (ninguém deve enviar nada a ele).
struct ClienteConectado {
    int socket = -1;
    Jogador jogador;
};