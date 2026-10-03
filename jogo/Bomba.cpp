#include "Carta.hpp"
#include "ServidorTCP.hpp"
#include<algorithm>
#include<vector>
class Bomba : public Carta{
    
    
    void Bomba::aplicarEfeito(ServidorTCP& servidor, std::shared_ptr<ClienteConectado> jogadorAlvo) {
       if (!jogadorAlvo) return;// falhou

    // 1. Procura na mão do jogador se existe uma carta com tipo Desarme
    auto& mao = (*jogadorAlvo).jogador.getMao(); // vetor de std::shared_ptr<Carta>
    auto it = std::find_if(mao.begin(), mao.end(), [](const std::unique_ptr<Carta>& c) {
        return c->getTipo() == TipoCarta::Desarme;
    });

    if (it != mao.end()) {
        // --- PILHA DE DESCARTE : olhar aqui pra implementar
        // Se a tua pilha de descarte for um vetor de ponteiros no servidor (ex: servidor.getPilhaDescarte()),
        // podes transferir a carta jogada usando std::move antes de apagar da mão:
        // servidor.getPilhaDescarte().push_back(std::move(*it));

        // Remove o elemento da mão (agora já transferido ou para ser destruído)
        mao.erase(it);

        servidor.broadcast("DESARME " + std::to_string(jogadorAlvo->jogador.getId()) + "\n");
        
        // Opcional: devolver a bomba ao baralho principal em uma posição escolhida
        // servidor.devolverBombaAoBaralho();
        
        return; // Jogador desarmou a bomba com sucesso
    }

    // 2. Se não tinha Desarme, elimina o jogador e notifica a sala
    jogadorAlvo->jogador.eliminar();
    servidor.broadcast("EXPLOSAO " + std::to_string(jogadorAlvo->jogador.getId()) + "\n");
    servidor.verificarFimDeJogo();
    }


};