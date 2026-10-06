#include "Jogador.hpp"

#include <utility>

// Adiciona uma carta à mão do jogador
void Jogador::adicionarCartaMao(std::unique_ptr<Carta> carta) {
    cartas.push_back(std::move(carta));
}

// Remove e retorna uma carta da mão pelo índice informado
std::unique_ptr<Carta> Jogador::removerCartaMao(size_t indice) {
    if (indice >= cartas.size()) return nullptr;

    auto carta = std::move(cartas[indice]);
    cartas.erase(cartas.begin() + static_cast<std::ptrdiff_t>(indice));
    return carta;
}

// Reinicia o estado do jogador para uma nova partida no lobby
void Jogador::reiniciarParaLobby() {
    cartas.clear();
    pronto = false;
    vivo = true;
}
