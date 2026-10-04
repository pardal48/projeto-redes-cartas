#include "Jogador.hpp"
#include "Carta.hpp"

Jogador::Jogador() {
    // Constructor implementation
}

Jogador::~Jogador() {
    // Destructor implementation
}

void Jogador::setId(int id) { this->id = id; }
int Jogador::getId() const { return id; }

std::string Jogador::getNome() const { return nome; }
void Jogador::setNome(const std::string& nome) { this->nome = nome; }

void Jogador::setPronto(bool pronto) { this->pronto = pronto; }
bool Jogador::getPronto() const { return pronto; }

bool Jogador::estaVivo() const { return vivo; }
void Jogador::eliminar() { vivo = false; }

const std::vector<std::unique_ptr<Carta>>& Jogador ::getMao()const{ return cartas; }

void Jogador::adicionarCartaMao(std::unique_ptr<Carta> carta) {
    cartas.push_back(std::move(carta));
}

std::unique_ptr<Carta> Jogador::removerCartaMao(size_t indice) {
    if (indice >= cartas.size()) return nullptr; // 1. Validação de segurança

    auto carta = std::move(cartas[indice]);      // 2. Transferência de posse
    cartas.erase(cartas.begin() + indice);           // 3. Limpeza do vetor

    return carta;                             // 4. Retorno do ponteiro
}