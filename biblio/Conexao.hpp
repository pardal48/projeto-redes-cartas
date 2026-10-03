#pragma once
#include "Jogador.hpp"

struct ClienteConectado {

    int socket;
    Jogador jogador;
};

struct ServidorInfo {
    std::string nome;
    std::string endereco;
    int porta;
    int jogadores;
    int capacidade = 5;

    bool estahCheio() const {
        return jogadores >= capacidade;
    }

    /*ServidorInfo(const std::string& nome, int porta)
        : nome(nome), porta(porta), jogadores(0) {
        // Inicializa o endereço como "localhost" ou outro valor padrão
        endereco = "localhost";
    }*/
};