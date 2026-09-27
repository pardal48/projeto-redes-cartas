
CXXFLAGS = -Wall -Wextra -std=c++17 -Ibiblio

SERVIDOR = controle/servidor.cpp controle/servidorTCP.cpp
CLIENTE = controle/cliente.cpp controle/clienteTCP.cpp
all: servidor cliente

servidor: 
	g++ $(CXXFLAGS) $(SERVIDOR) -o bin/servidor

cliente:
	g++ $(CXXFLAGS) $(CLIENTE) -o bin/cliente

clean:
	rm -f servidor cliente
