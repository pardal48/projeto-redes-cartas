#include "TipoCarta.hpp"

namespace {
struct Info { TipoCarta tipo; const char* token; const char* nome; const char* descricao; };

const Info kTabela[] = {
    {TipoCarta::Ataque,       "ATACAR",     "Atacar",        "Termina o turno sem comprar; o proximo jogador faz 2 turnos."},
    {TipoCarta::Pular,        "PULAR",      "Pular",         "Termina 1 turno sem comprar."},
    {TipoCarta::Favor,        "FAVOR",      "Favor",         "Um jogador a sua escolha lhe entrega 1 carta (ele escolhe qual)."},
    {TipoCarta::Embaralhar,   "EMBARALHAR", "Embaralhar",    "Embaralha o baralho de compra."},
    {TipoCarta::Futuro,       "FUTURO",     "Ver o futuro",  "Veja em segredo as 3 primeiras cartas do baralho."},
    {TipoCarta::Nao,          "NAO",        "Nao",           "Anula qualquer acao (menos Bomba/Defuse); pode ser anulado por outro Nao."},
    {TipoCarta::GatoAranha,   "GATO1",      "Gato Aranha",   "Combo: 2 iguais roubam carta aleatoria; 3 iguais pedem uma carta."},
    {TipoCarta::GatoBarba,    "GATO2",      "Gato Barba",    "Combo: 2 iguais roubam carta aleatoria; 3 iguais pedem uma carta."},
    {TipoCarta::GatoBatata,   "GATO3",      "Gato Batata",   "Combo: 2 iguais roubam carta aleatoria; 3 iguais pedem uma carta."},
    {TipoCarta::GatoMelancia, "GATO4",      "Gato Melancia", "Combo: 2 iguais roubam carta aleatoria; 3 iguais pedem uma carta."},
    {TipoCarta::GatoTaco,     "GATO5",      "Gato Taco",     "Combo: 2 iguais roubam carta aleatoria; 3 iguais pedem uma carta."},
    {TipoCarta::Desarme,      "DEFUSE",     "Defuse",        "Salva voce de uma bomba; ela volta ao baralho em segredo."},
    {TipoCarta::Bomba,        "BOMBA",      "Gatinho Explosivo", "Voce explode, a menos que tenha um Defuse."},
};

const Info* buscar(TipoCarta tipo) {
    for (const auto& i : kTabela) if (i.tipo == tipo) return &i;
    return nullptr;
}
}  // namespace

const char* tipoParaToken(TipoCarta tipo) { const Info* i = buscar(tipo); return i ? i->token : "DESCONHECIDA"; }

bool tokenParaTipo(const std::string& token, TipoCarta& saida) {
    if (token == "DEFUSAR") { saida = TipoCarta::Desarme; return true; }
    for (const auto& i : kTabela)
        if (token == i.token) { saida = i.tipo; return true; }
    return false;
}

std::string tipoParaNomeLegivel(TipoCarta tipo) { const Info* i = buscar(tipo); return i ? i->nome : "?"; }
std::string tipoParaDescricao(TipoCarta tipo) { const Info* i = buscar(tipo); return i ? i->descricao : ""; }
bool ehGato(TipoCarta tipo) { return tipo >= TipoCarta::GatoAranha && tipo <= TipoCarta::GatoTaco; }
