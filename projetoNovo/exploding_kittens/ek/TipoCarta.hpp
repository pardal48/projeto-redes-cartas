#pragma once
// TipoCarta.hpp - enum dos tipos de carta + utilitários de texto. É um header LEVE: o cliente
// também o usa (para traduzir os tokens de rede em nomes legíveis) sem puxar o resto do servidor.
#include <string>

enum class TipoCarta {
    Ataque, Pular, Favor, Embaralhar, Futuro, Nao,
    GatoAranha, GatoBarba, GatoBatata, GatoMelancia, GatoTaco,  // os 5 gatos são contíguos (ehGato)
    Desarme,  // "Defusar"
    Bomba     // Exploding Kitten
};

// Token usado no protocolo e como nome interno da carta: "ATACAR", "GATO1", "DEFUSE"...
const char* tipoParaToken(TipoCarta tipo);
// Inverso. Aceita também "DEFUSAR" como apelido de DEFUSE.
bool tokenParaTipo(const std::string& token, TipoCarta& saida);
std::string tipoParaNomeLegivel(TipoCarta tipo);   // "Ver o futuro", "Gato Taco"...
std::string tipoParaDescricao(TipoCarta tipo);
bool ehGato(TipoCarta tipo);
