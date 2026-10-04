#pragma once
// Carta.hpp - classe base (abstrata) das cartas. Cada tipo de carta com efeito próprio é uma
// subclasse (CartaPular, CartaAtaque...) criada pela fábrica Carta::criar().
#include <memory>
#include <string>

#include "TipoCarta.hpp"

class ServidorTCP;
struct ClienteConectado;

class Carta {
public:
    Carta(int id, std::string nome, std::string descricao, TipoCarta tipo);
    virtual ~Carta();

    int getId() const;               // id ÚNICO da carta física na partida (é o que o cliente envia em JOGAR)
    std::string getNome() const;
    std::string getDescricao() const;
    TipoCarta getTipo() const;
    void setId(int id);

    // Efeito da carta, chamado pela Partida DEPOIS que a janela do "Não" fechou sem anulá-la.
    // 'jogadorAlvo' é o alvo escolhido (nullptr se a carta não usa alvo). Quem jogou a carta é o
    // jogador da vez; o estado do jogo é acessado por servidor.partidaAtiva().
    virtual void aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> jogadorAlvo) = 0;

    // Metadados que a Partida usa para validar JOGAR sem conhecer cada tipo de carta:
    virtual bool jogavelSozinha() const { return false; }  // pode ser jogada uma a uma?
    virtual bool precisaAlvo() const { return false; }     // exige o argumento <alvo>?

    // Fábrica: devolve a subclasse certa para o tipo.
    static std::unique_ptr<Carta> criar(int id, const std::string& nome, const std::string& desc, TipoCarta tipo);

private:
    int id;
    std::string nome;
    std::string descricao;
    TipoCarta tipo;
};
