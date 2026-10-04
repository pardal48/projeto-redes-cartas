#include "Jogador.hpp"

#include <utility>

void Jogador::adicionarCartaMao(std::unique_ptr<Carta> carta) {
    cartas.push_back(std::move(carta));
}

std::unique_ptr<Carta> Jogador::removerCartaMao(size_t indice) {
    if (indice >= cartas.size()) return nullptr;

    auto carta = std::move(cartas[indice]);
    cartas.erase(cartas.begin() + static_cast<std::ptrdiff_t>(indice));
    return carta;
}

void Jogador::reiniciarParaLobby() {
    cartas.clear();
    pronto = false;
    vivo = true;
}