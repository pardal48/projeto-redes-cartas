# Exploding Kittens em C++ (Cliente/Servidor TCP, Linux)

Jogo de terminal para 2 a 5 jogadores. Servidor multithread com `poll` + uma thread por cliente, motor de regras isolado na classe `Partida`, efeitos das cartas em subclasses de `Carta` criadas por uma fábrica.

## Compilar e rodar

```bash
make                 # gera ./servidor e ./cliente   (g++ -std=c++17 -pthread)
./servidor           # pergunta o nome da sala e escuta na porta 5000
./cliente            # um terminal por jogador (conecta em localhost:5000)
make teste_partida   # testes do motor (ASan/UBSan)
```

No lobby: `P` + Enter alterna pronto/espera, `S` + Enter sai. Com 2 a 5 jogadores **todos** prontos começa a contagem 5…1; qualquer mudança de status (ou entrada/saída de alguém) interrompe a contagem. Na partida, digite `ajuda`.

## Arquivos

| Arquivo | Papel |
|---|---|
| `Servidor.cpp`, `Cliente.cpp` | `main` de cada lado (`Servidor.cpp` ficou **igual** ao seu) |
| `ServidorTCP.hpp/.cpp` | sockets, lobby, contagem, thread do relógio, ponte para a `Partida` |
| `Partida.hpp/.cpp` | **regras do jogo**: turnos, baralho, pilha de efeitos, janela do "Não", favor, bomba, desconexões |
| `Carta.hpp/.cpp` | classe base abstrata `Carta` (`aplicarEfeito`, `jogavelSozinha`, `precisaAlvo`) |
| `CartaFabrica.cpp` | fábrica `Carta::criar` (separada para o cliente não linkar o servidor) |
| `CartaNormal/CartaPular/CartaAtaque/CartaEmbaralhar/CartaFuturo/CartaFavor/Bomba.hpp` | uma subclasse por efeito |
| `TipoCarta.hpp/.cpp` | enum `TipoCarta`, tokens de rede, nomes legíveis (usado também pelo cliente) |
| `Jogador.hpp/.cpp` | id, nome, pronto, vivo e **a mão** (`vector<unique_ptr<Carta>>`) |
| `ClienteTCP.hpp/.cpp`, `Interface.hpp/.cpp` | rede e terminal do cliente |
| `Conexao.hpp` | `ServidorInfo` |
| `tests/` | `test_partida.cpp` (motor) e `bots.py` (integração pela rede) |

## Regras implementadas

* **Setup:** baralho base de 46 cartas (Atacar 4, Favor 4, Não 5, Embaralhar 4, Pular 4, Ver o futuro 5, 5 gatos × 4). Cada jogador recebe 1 Defuse + 7 cartas. Depois entram **jogadores − 1 bombas** e `min(2, 6 − jogadores)` Defuses extras, e o baralho é embaralhado.
* **Turno:** lista circular (`deque`); no turno pode-se jogar qualquer número de cartas e termina-se comprando (`COMPRAR`) ou com Pular/Atacar.
* **Atacar:** encerra o turno sem comprar e o próximo faz 2 turnos. Se o atacante já estava sob ataque, acumula: *turnos que ainda devia + 2* (completar 1 turno e então atacar dá 3).
* **Pular:** encerra 1 turno (dentro de um ataque, só 1 dos 2).
* **Favor:** o alvo escolhe a carta (`DAR <id>`); se não responder em 30 s o servidor sorteia.
* **Combos (só gatos, como no enunciado):** 2 iguais roubam carta aleatória; 3 iguais pedem uma carta nomeada (se o alvo não tiver, nada acontece).
* **Não:** toda carta de efeito (incluindo combos) abre uma janela de **5 s** no servidor. Qualquer jogador vivo pode jogar `JOGAR_NAO`; cada Não reabre a janela por mais 5 s. Número ímpar de Nãos anula a ação (as cartas vão ao descarte).
* **Bomba:** com Defuse, o Defuse vai ao descarte e o jogador escolhe a posição (`POSICAO <n>`, 0 = topo; 30 s, depois sorteia). Sem Defuse explode: mão e bomba vão ao descarte e ele sai da lista de turnos.
* **Fim:** sobra 1 jogador → `FIM_DE_JOGO VENCEDOR <id>` e **todos voltam ao lobby** (continuam conectados, não prontos).
* **Desconexão de jogador vivo:** cartas voltam ao baralho e ele é embaralhado; o Defuse sai do jogo; uma bomba aleatória é removida (bombas = vivos − 1); ações pendentes que dependiam dele são canceladas; se era a vez dele passa ao próximo. **Morto** que desconecta: só `SAIU <id> MORTO`.

## Protocolo (texto, uma mensagem por linha, `\n`)

Nomes de jogador são alfanuméricos (1–12), então não há espaços dentro dos campos. `<id>` de jogador começa em 1 (0 = ninguém). `<idCarta>` é um id **único por carta física** (aparece em `MAO`).

### Cliente → servidor

| Mensagem | Quando | Significado |
|---|---|---|
| `NOME <nome>` | lobby | escolhe o nome |
| `PRONTO` / `ESPERA` | lobby | muda o status |
| `SAIR` | sempre | abandona (servidor responde `ATE_LOGO` e fecha) |
| `MESA`, `MAO` | partida, qualquer um | consultas |
| `COMPRAR` | sua vez | compra e encerra o turno |
| `JOGAR <ids> [alvo] [TIPO]` | sua vez | `<ids>` = 1 a 3 `idCarta` separados por vírgula (`5,9,14`). 1 carta: efeito (Favor exige `alvo`). 2 gatos iguais: `alvo`. 3 gatos iguais: `alvo` e `TIPO` (`DEFUSE`, `NAO`, `PULAR`, `ATACAR`, `FAVOR`, `EMBARALHAR`, `FUTURO`, `GATO1`…`GATO5`) |
| `JOGAR_NAO` | durante a janela | joga um Não |
| `DAR <idCarta>` | quando recebeu `DAR_CARTA` | entrega a carta do Favor |
| `POSICAO <n>` | quando recebeu `REINSERIR` | reinsere a bomba (0 = topo … N = fundo) |

### Servidor → cliente

Lobby: `OK <id>` · `OK NOME_ACEITO` · `LOBBY <id>:<nome>:<0|1>;…` · `CONTAGEM <n>` · `CONTAGEM_CANCELADA` · `INICIAR` · `ERRO LOBBY_CHEIO` (e fecha; também se a partida já começou).

Partida, para todos: `JOGO_INICIADO` · `ORDEM <id…>` · `TURNO <id> <turnosPendentes>` · `JOGOU <id> <TIPO[,TIPO]> <alvo|0> <pedido|->` · `PERGUNTA_NAO <id> <TIPO> <segundos>` · `JOGOU_NAO <id>` · `CANCELADO` / `APLICADO` · `EMBARALHOU <id>` · `FAVOR_PEDIDO <de> <para>` · `FAVOR_CONCLUIDO <de> <para>` · `FAVOR_SEM_CARTAS …` · `FAVOR_CANCELADO` · `ROUBO <ladrao> <vitima> <0|1>` · `COMPRA <id> <tamBaralho>` · `EXPLOSAO <id>` (comprou bomba) · `DEFUSOU <id>` · `BOMBA_REINSERIDA <id>` · `MORREU <id>` · `SAIU <id> <VIVO|MORTO>` · `AVISO <texto>` · `FIM_DE_JOGO VENCEDOR <id>` ou `FIM_DE_JOGO EMPATE`.

Partida, privadas: `MAO <idCarta>:<TIPO> …` · `STATUS VEZ:<id> TURNOS:<n> JOGADORES_VIVOS:<n> BARALHO:<n> DESCARTE:<n> TOPO:<TIPO|-> FASE:<TURNO|JANELA|FAVOR|BOMBA|FIM> JOGADORES:<id>:<vivo>:<nCartas>;…` · `FUTURO <t1> <t2> <t3>` · `COMPROU <TIPO>` · `RECEBEU <TIPO> <ROUBO|FAVOR>` · `PERDEU <TIPO>` · `DAR_CARTA <solicitante> <segundos>` · `REINSERIR <tamBaralho> <segundos>`.

Erros (`ERRO <codigo>`): `COMANDO_INVALIDO`, `COMANDO_DESCONHECIDO`, `ARGUMENTOS`, `JOGO_TERMINADO`, `VOCE_ESTA_MORTO`, `NAO_E_SEU_TURNO`, `FASE_INVALIDA`, `AGUARDANDO_RESPOSTA_NAO`, `NAO_FORA_DA_JANELA`, `VOCE_NAO_TEM_A_CARTA_NAO`, `INDICE_INVALIDO` (carta que não está na mão, ids repetidos ou malformados), `COMBO_INVALIDO`, `CARTA_NAO_JOGAVEL`, `ALVO_INVALIDO`, `ALVO_SEM_CARTAS`, `CARTA_PEDIDA_INVALIDA`, `NAO_E_SUA_ESCOLHA`, `POSICAO_INVALIDA`, `NUMERO_JOGADORES_INVALIDO`, `NOME_INVALIDO`, `JA_TEM_NOME`, `LOBBY_CHEIO`.

Todo número vindo do cliente é lido por um parser que aceita só dígitos (até 9) e é conferido contra a mão, os jogadores vivos e o tamanho do baralho; mensagem malformada devolve `ERRO`, nunca derruba o servidor. Linhas sem `\n` acima de 4096 bytes derrubam apenas aquele cliente.

Exemplo (Ana joga Atacar, Bia anula com Não):

```
Ana → JOGAR 12
S→* JOGOU 1 ATACAR 0 -        S→* PERGUNTA_NAO 1 ATACAR 5
Bia → JOGAR_NAO
S→* JOGOU_NAO 2               S→* PERGUNTA_NAO 2 NAO 5       (janela reaberta)
        … 5 s sem novo Não …
S→* CANCELADO                 (1 Não = ímpar: Ana continua na vez)
```

## Concorrência

* **Um mutex** (`ServidorTCP::mtx`) protege `clientes`, `jogoIniciado`, a contagem do lobby e a `Partida` inteira. A `Partida` não tem mutex próprio: só é chamada com ele travado (recv-threads, relógio, `removerCliente`).
* **Timers não-bloqueantes:** a `Partida` apenas guarda um prazo (`proximoPrazo()`); a thread do relógio espera em `condition_variable::wait_until` (que **libera** o mutex), é acordada por `cv.notify_all()` quando algo muda e chama `processarTempo()` ao vencer. A thread de `recv` nunca espera pelo timer.
* O prazo é lido e a espera começa sob o mesmo `unique_lock`, então não há "wake-up perdido".
* `send` sob lock usa `SO_SNDTIMEO` de 3 s, para um cliente travado não congelar o servidor. Cliente removido tem `socket = -1` e nunca recebe envio (evita escrever em fd reaproveitado).
* Cliente: a thread de recepção chama `aoAtualizar`/`aoEvento` **fora** do mutex interno; a `Interface` serializa a tela com `mtxTela`.

## Mudanças em relação aos seus arquivos

Arquivos novos de headers: **`Partida.hpp`, `Carta.hpp`, `Jogador.hpp`, `ServidorTCP.hpp`, `Conexao.hpp`, `ClienteTCP.hpp`, `Interface.hpp` não vieram no upload e foram reconstruídos** a partir do uso nos `.cpp`; compare com os seus.

Decisões que alteram o que você enviou:

1. **Janela do Não por timer (5 s) em vez de "todos respondem".** Seu `PERGUNTA_NAO`/`JOGAR_NAO`/`PASSO` esperava resposta de todos, o que trava se um jogador ficar parado, e o enunciado pede timer de 5 s. Mantive `PERGUNTA_NAO` (agora com o campo `<segundos>`), `JOGAR_NAO`, `JOGOU_NAO`, `CANCELADO` e a regra de paridade da `pilhaEfeitos`; `PASSO` deixou de existir.
2. **`JOGAR` usa o id único da carta, não o índice na mão.** Os ids eram iguais por tipo; agora cada carta física tem id próprio. Evita jogar a carta errada se a mão mudar (roubo, favor) entre o comando e a chegada ao servidor.
3. **`aplicarEfeito(ServidorTCP&, shared_ptr<ClienteConectado>)` mantido.** O efeito chega ao estado do jogo por `servidor.partidaAtiva()`. Foram acrescentados à `Carta` os virtuais `jogavelSozinha()` e `precisaAlvo()` (a `Partida` valida `JOGAR` sem conhecer cada tipo) e as subclasses `CartaAtaque/Embaralhar/Futuro/Favor`. Combos (2/3 gatos) ficam na `Partida`.
4. **`Carta::criar` foi movida para `CartaFabrica.cpp`** (mesmo código, mais os novos tipos), porque o cliente linka `Carta.cpp` por causa da mão do `Jogador` e não pode depender de `Partida`/`ServidorTCP`.
5. **`Partida::jogadores` agora é cópia dos `shared_ptr`**, não referência à lista do servidor: `ordemTurnos` guarda índices e o servidor apaga clientes da lista ao desconectarem, o que deslocava os índices.
6. **Mantido do seu código:** nomes `DEFUSE`/`GATO1…5` (mapeados para seus `TipoCarta`), `ordemTurnos` como `deque` que gira, `pilhaEfeitos`, `removerJogador(id, desconexao, servidor)`, mensagens `JOGO_INICIADO`, `TURNO`, `JOGOU`, `EXPLOSAO`, `DEFUSOU`, `MORREU`, `EMBARALHOU`, `FIM_DE_JOGO VENCEDOR`, `STATUS`. Jogador inicial = primeiro da lista, Defuses extras entrando após a distribuição (como no seu `iniciar`). `DEFUSAR` é aceito como apelido de `DEFUSE` no combo de 3.

Defeitos corrigidos:

* `jogarCarta` e `comprarCarta` usavam `carta->getNome()` **depois** de `std::move(carta)` (ponteiro nulo → crash).
* Após explosão a vez rodava um jogador a mais (o removido já não estava na frente); o Atacar contava turnos de forma errada; `Jogador::eliminar()` nunca era chamado; Favor, Ver o futuro, combos e alvo não estavam implementados; a bomba voltava em posição aleatória sem o jogador escolher.
* `ServidorTCP::registrar` não travava o mutex (corrida em `clientes`); `atenderCliente` testava `n <= 0 && errno == EINTR` (com `n == 0` e `errno` antigo podia girar em falso; agora `n < 0`); o construtor continuava após falha de `socket/bind/listen`; o cliente prometia "nome já em uso" mas o servidor não verificava (agora verifica, sem distinguir maiúsculas).
* `ServidorTCP::jogadoresVivosCount` foi removida e `verificarFimDeJogo` reescrita (a original usava `FIM_DE_JOGO <id>` diferente do da `Partida`; agora só existe `FIM_DE_JOGO VENCEDOR <id>` e o retorno ao lobby é feito nela). `processarComandoJogo` deixou de ser um stub.

## Validação

* `make teste_partida` (ASan + UBSan): usa o `ServidorTCP` real com `socketpair` no lugar dos clientes. **86 verificações de cenários** (setup de 2 a 5 jogadores, Atacar simples/acumulado/3 turnos, Pular em ataque, Não ímpar/par, combos de 2 e 3, Favor e timeout, bomba com/sem Defuse, reinserção, fim e retorno ao lobby, desconexões em cada fase) e **600 partidas aleatórias** com comandos malformados e desconexões abruptas, checando a cada passo: ids de carta únicos, bombas = vivos − 1, ordem de turnos = vivos, mortos sem cartas. Resultado: 0 falhas, todas as partidas terminaram.
* `tests/bots.py`: bots Python pela rede contra o servidor compilado com **ThreadSanitizer** e janelas curtas (`make servidor_teste`): partidas de 2, 3, 4 e 5 jogadores, duas partidas seguidas (retorno ao lobby) e desconexões abruptas. Vencedor consistente para todos; **nenhum aviso do TSan**.
* Teste manual automatizado com **dois `./cliente` reais** e servidor real: lobby, contagem interrompida e reiniciada, partida, mensagens de erro, abandono (o outro vence e volta ao lobby) e saída com código 0.

## Limitações conhecidas

* Variante rápida de 2–3 jogadores do manual não implementada. Combos valem só para gatos (como no enunciado; o manual permite qualquer par).
* O Defuse é usado automaticamente ao comprar a bomba (sem escolha de "não usar").
* A entrada do cliente é por linha (`poll` + `getline`): o texto que o jogador está digitando pode ser interrompido visualmente por mensagens do servidor (a linha continua válida).
* `ServidorTCP::threads` cresce uma entrada por conexão aceita e só é limpo no encerramento (herdado do original).
* Não há autenticação nem criptografia (rede local/aula).
