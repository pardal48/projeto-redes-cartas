CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -pthread

COMUM    = Carta.cpp Jogador.cpp TipoCarta.cpp
SERVIDOR = Servidor.cpp ServidorTCP.cpp Partida.cpp CartaFabrica.cpp $(COMUM)
CLIENTE  = Cliente.cpp ClienteTCP.cpp Interface.cpp TipoCarta.cpp Jogador.cpp Carta.cpp

all: servidor cliente

servidor: $(SERVIDOR) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVIDOR)

cliente: $(CLIENTE) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENTE)

# Servidor com prazos curtos (janela do Nao = 300 ms) para os testes automaticos
servidor_teste: $(SERVIDOR) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -DEK_JANELA_NAO_MS=300 -DEK_RESPOSTA_MS=1500 -o $@ $(SERVIDOR)

clean:
	rm -f servidor cliente servidor_teste teste_partida *.o

.PHONY: all clean teste_partida

# Teste do motor (sem rede real): cenarios das regras + fuzz com invariantes, sob ASan/UBSan
teste_partida: tests/test_partida.cpp ServidorTCP.cpp Partida.cpp CartaFabrica.cpp $(COMUM) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g -o $@ tests/test_partida.cpp ServidorTCP.cpp Partida.cpp CartaFabrica.cpp $(COMUM)
	./$@
