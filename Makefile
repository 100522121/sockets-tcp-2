CC      = gcc
CFLAGS  = -Wall -fPIC -g -Iinclude

# Fuentes
CLAVES_C   = src/claves.c
PROXY_C    = src/proxy-sock.c
SERVIDOR_C = src/servidor-sock.c
CLIENTE_C  = tests/app-cliente.c

# Cabeceras
CLAVES_H    = include/claves.h
PROTOCOLO_H = include/protocolo.h

# Artefactos
LIB_CLAVES = libclaves.so
LIB_PROXY  = libproxyclaves.so

.PHONY: all clean

all: $(LIB_CLAVES) $(LIB_PROXY) servidor cliente

# Biblioteca de almacenamiento
$(LIB_CLAVES): $(CLAVES_C) $(CLAVES_H)
	$(CC) $(CFLAGS) -shared -o $@ $(CLAVES_C) -lpthread

# Biblioteca cliente
$(LIB_PROXY): $(PROXY_C) $(CLAVES_H) $(PROTOCOLO_H)
	$(CC) $(CFLAGS) -shared -o $@ $(PROXY_C)

# Servidor TCP concurrente
servidor: $(SERVIDOR_C) $(LIB_CLAVES) $(CLAVES_H) $(PROTOCOLO_H)
	$(CC) $(CFLAGS) $(SERVIDOR_C) -o $@ $(LIB_CLAVES) -lpthread -Wl,-rpath,.

# Ejecutable cliente de pruebas
cliente: $(CLIENTE_C) $(LIB_PROXY) $(CLAVES_H)
	$(CC) $(CFLAGS) $(CLIENTE_C) -o $@ $(LIB_PROXY) -Wl,-rpath,.

# Limpieza
clean:
	rm -f servidor cliente $(LIB_CLAVES) $(LIB_PROXY) *.o