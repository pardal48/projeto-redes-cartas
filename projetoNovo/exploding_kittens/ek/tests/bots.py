#!/usr/bin/env python3
"""Teste de integracao: N bots falam o protocolo real com o servidor.
Uso: python3 tests/bots.py <porta> <n_bots> [partidas=1] [prob_desconexao=0]
Cada bot joga aleatoriamente ate FIM_DE_JOGO; depois volta ao lobby (PRONTO) para a proxima partida.
Desconexoes abruptas so sao simuladas quando partidas == 1."""
import random, socket, sys, threading, time

PORTA = int(sys.argv[1]); N = int(sys.argv[2])
PARTIDAS = int(sys.argv[3]) if len(sys.argv) > 3 else 1
P_DESC = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
vencedores = {}   # (bot, partida) -> id do vencedor | 'desconectou'
erros_total = {}

class Bot(threading.Thread):
    def __init__(self, idx):
        super().__init__(daemon=True)
        self.idx = idx; self.nome = f"bot{idx}"
        self.s = socket.create_connection(("127.0.0.1", PORTA)); self.s.settimeout(0.1)
        self.buf = ""; self.me = None; self.hand = {}; self.turn = None; self.alive = set()
        self.bloqueio = 0.0; self.jogos = 0; self.saiu = False

    def send(self, m): self.s.sendall((m + "\n").encode())

    def linhas(self):
        try:
            d = self.s.recv(4096)
            if not d: return None
            self.buf += d.decode()
        except socket.timeout: pass
        out = []
        while "\n" in self.buf:
            l, self.buf = self.buf.split("\n", 1); out.append(l.strip())
        return out

    def agir(self):
        if self.turn != self.me or time.time() < self.bloqueio or self.me not in self.alive: return
        outros = [i for i in self.alive if i != self.me]
        tipos = {}
        for cid, t in self.hand.items(): tipos.setdefault(t, []).append(cid)
        opcoes = []
        for t in ("ATACAR", "PULAR", "EMBARALHAR", "FUTURO"):
            if t in tipos: opcoes.append(f"JOGAR {tipos[t][0]}")
        if "FAVOR" in tipos and outros: opcoes.append(f"JOGAR {tipos['FAVOR'][0]} {random.choice(outros)}")
        for t, ids in tipos.items():
            if t.startswith("GATO") and len(ids) >= 2 and outros:
                opcoes.append(f"JOGAR {ids[0]},{ids[1]} {random.choice(outros)}")
                if len(ids) >= 3:
                    opcoes.append(f"JOGAR {ids[0]},{ids[1]},{ids[2]} {random.choice(outros)} {random.choice(['DEFUSE','NAO','PULAR','GATO1'])}")
        if opcoes and random.random() < 0.5: self.send(random.choice(opcoes)); self.bloqueio = time.time() + 0.8
        else: self.send("COMPRAR"); self.bloqueio = time.time() + 0.3

    def run(self):
        try:
            self.send(f"NOME {self.nome}")
            while self.jogos < PARTIDAS and not self.saiu:
                ls = self.linhas()
                if ls is None: break
                for l in ls: self.tratar(l)
                if PARTIDAS == 1 and P_DESC and self.me in self.alive and self.turn is not None and random.random() < P_DESC / 50:
                    self.saiu = True; self.s.close(); vencedores[(self.idx, 0)] = "desconectou"; return
                self.agir()
        except Exception as e:
            vencedores[(self.idx, -1)] = f"EXC {e!r}"

    def tratar(self, l):
        t = l.split()
        if not t: return
        c = t[0]
        if c == "OK" and len(t) == 2 and t[1].isdigit(): self.me = int(t[1])
        elif l == "OK NOME_ACEITO": self.send("PRONTO")
        elif c == "ORDEM": self.alive = {int(x) for x in t[1:]}
        elif c == "MAO": self.hand = {int(x.split(":")[0]): x.split(":")[1] for x in t[1:]}
        elif c == "TURNO": self.turn = int(t[1])
        elif c in ("MORREU", "SAIU"): self.alive.discard(int(t[1]))
        elif c == "PERGUNTA_NAO":
            self.bloqueio = time.time() + 0.6
            if "NAO" in self.hand.values() and random.random() < 0.4: self.send("JOGAR_NAO")
        elif c == "DAR_CARTA" and self.hand: self.send(f"DAR {random.choice(list(self.hand))}")
        elif c == "REINSERIR": self.send(f"POSICAO {random.randint(0, int(t[1]))}")
        elif c == "FIM_DE_JOGO":
            vencedores[(self.idx, self.jogos)] = int(t[2]) if len(t) > 2 and t[2].isdigit() else t[-1]
            self.jogos += 1; self.hand = {}; self.turn = None; self.alive = set()
            if self.jogos < PARTIDAS: self.send("PRONTO")   # o servidor ja voltou ao lobby
        elif c == "ERRO": erros_total[t[1]] = erros_total.get(t[1], 0) + 1

bots = [Bot(i) for i in range(N)]
for b in bots: b.start()
limite = time.time() + 150
while time.time() < limite and any(b.is_alive() for b in bots): time.sleep(0.2)
travados = [b.nome for b in bots if b.is_alive()]
print("vencedores:", dict(sorted(vencedores.items())))
print("erros recebidos (esperados em jogo aleatorio):", erros_total)
if travados: print("TRAVOU:", travados); sys.exit(1)
if any(isinstance(v, str) and v.startswith("EXC") for v in vencedores.values()): print("EXCECAO"); sys.exit(1)
for g in range(PARTIDAS):
    fins = {v for (i, gg), v in vencedores.items() if gg == g and v != "desconectou"}
    if len(fins) != 1: print(f"partida {g}: vencedor divergente/ausente {fins}"); sys.exit(1)
sys.exit(0)
