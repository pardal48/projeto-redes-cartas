
CXXFLAGS = -Wall -Wextra -std=c++17 -Ibiblio

SERVIDOR = controle/Servidor.cpp controle/ServidorTCP.cpp
CLIENTE = controle/Cliente.cpp controle/ClienteTCP.cpp
JOGO = jogo/Jogador.cpp jogo/Carta.cpp
INTERACAO= interacao/Interface.cpp
all: servidor cliente jogo

servidor: 
	g++ $(CXXFLAGS) $(SERVIDOR) -o bin/servidor

cliente:
	g++ $(CXXFLAGS) $(CLIENTE) $(JOGO) $(INTERACAO) -o bin/cliente

clean:
	rm -f servidor cliente
