Jogo multijogador de terminal: um servidor hospeda o lobby e a partida;
2 a 5 clientes se conectam, escolhem um nome, ficam *prontos* e jogam
por turnos. Vence o último jogador que não explodir.

# Grupo

::: center
  ------------------------------ ----------
  Alexandre Coura Serravite        15507531
  Bruno Baremaker Moraes           15443854
  Felipe Freitas Maia              15451360
  Matheus Watanabe de Vilhena      15479767
  Rafael Olandoski de Oliveira     15485382
  ------------------------------ ----------
:::

# Vídeo de Explicação

Fizemos um vídeo explicando o funcionamento do jogo e do projeto,
[disponível neste link.](https://youtu.be/k6WgRKneVaA)

# Ambiente

- **Sistema operacional:** Linux Mint 22.2 (base Ubuntu 24.04 LTS)

- **Compilador:** g++ (GCC) 13.3.0, padrão C++17

- **Dependências:** biblioteca padrão e APIs POSIX (, , , ); **não
  compila no Windows**.

# Estrutura

  -- -----------------------------
     , , , ,
     ,
     (entrada/saída do terminal)
     cabeçalhos
     executáveis gerados
  -- -----------------------------

# Compilação e execução

``` {.bash language="bash"}
make -f Makefile                # gera bin/servidor e bin/cliente

./bin/servidor [porta]          # padrao: 5000; pede o nome do servidor
./bin/cliente [host] [porta]    # padrao: localhost 5000
```

Use um terminal para o servidor e um por jogador. O servidor encerra com
**Ctrl+C**.

# Como jogar

**Fluxo:** tela inicial ( sai, qualquer outra tecla entra) $\to$ nome
(até 12 letras/números, único, sem distinguir maiúsculas) $\to$ lobby
(+Enter alterna pronto, +Enter sai) $\to$ partida. Ela começa quando há
de 2 a 5 jogadores e todos estão prontos.

**Regras:** cada jogador começa com 1 + 7 cartas. No seu turno, joga
cartas (opcional) e termina **comprando**. Ao comprar uma bomba, o a
devolve ao baralho em posição aleatória; sem ele, o jogador é eliminado.
Toda carta de ação ou combo abre uma janela em que os outros podem
cancelá-la com .

::: center
  -- -------------------------------------- ---- ---------------------------
     próximo joga 2 turnos                       vê as 3 próximas cartas
     termina o turno sem comprar                 cancela uma ação (reação)
     alvo entrega uma carta à sua escolha   --   só em combos
     embaralha o baralho                         
  -- -------------------------------------- ---- ---------------------------
:::

**Combos** (cartas do mesmo tipo): com 2 cartas rouba-se uma carta
*aleatória* de um jogador; com 3, escolhe-se o jogador *e o tipo* da
carta (falha se o alvo não a tiver).

::: center
  **Comando (não diferencia caixa)**   **Uso**
  ------------------------------------ ---------------------------------------------------------------------
                                       compra e encerra o turno
                                       joga a carta de índice da mão
                                       combo de 2 ou 3 cartas (índices distintos)
  /                                    responde à janela de reação
  nome / número / tipo                 escolhe alvo, carta a entregar ou tipo a roubar (quando solicitado)
  , ,                                  redesenha a mesa, mostra o descarte, sai
:::

# Protocolo

TCP/IPv4, mensagens de texto **terminadas em** .

# Verificações de falha

## Conexão {#conexão .unnumbered}

- **Recusa:** lobby cheio (5) ou partida em andamento $\Rightarrow$ e .

- **Desconexão de cliente** (): o socket é fechado, o jogador sai da
  lista e é eliminado da partida (que pode terminar), e o lobby é
  atualizado

## Transmissão {#transmissão .unnumbered}

- **Envio completo:** em loop até enviar todos os bytes, repetindo em ;
  retorno é falha. evita em socket fechado.

- **Delimitação:** como o TCP é um fluxo de bytes, os dados são
  acumulados em buffer e só **linhas completas** são processadas (trata
  mensagens fragmentadas ou coladas; remove ).

- **Abuso:** mais de 4096 bytes sem $\Rightarrow$ conexão encerrada.

- **Concorrência:** mutex no servidor; no cliente, serializa e protege o
  estado. Os *callbacks* rodam fora do lock (evita deadlock).

- **Validação:** nomes, comandos desconhecidos, índices, combos, alvos e
  vez do jogador são checados no servidor; o cliente ignora mensagens
  desconhecidas e descarta itens malformados do
