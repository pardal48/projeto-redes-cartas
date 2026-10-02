
CXXFLAGS = -Wall -Wextra -std=c++17 -Ibiblio

SERVIDOR = controle/servidor.cpp controle/servidorTCP.cpp
CLIENTE = controle/cliente.cpp controle/clienteTCP.cpp
JOGO = jogo/jogador.cpp jogo/Carta.cpp
INTERACAO= interacao/Interface.cpp
all: servidor cliente jogo

servidor: 
	g++ $(CXXFLAGS) $(SERVIDOR) -o bin/servidor

cliente:
	g++ $(CXXFLAGS) $(CLIENTE) $(JOGO) $(INTERACAO) -o bin/cliente

clean:
	rm -f servidor cliente
