# Variables de compilación
CC = gcc
CFLAGS = -Wall -fPIC -g -Iinclude

# Rutas de ficheros
PROTOCOLO_H = include/protocolo.h
CLAVES_H 	= include/claves.h
CLAVES_C 	= src/claves.c 

# Artefactos de salida
LIB_CLAVES  = libclaves.so
LIB_PROXY 	= libproxyclaves.so  
PROXY 		= src/proxy-sock.c
SERVIDOR	= src/servidor-sock.c 
CLIENTE 	= tests/app-cliente.c 

all: servidor cliente $(LIB_CLAVES) $(LIB_PROXY)
 
libclaves.so: $(CLAVES_C) $(CLAVES_H)
	$(CC) $(CFLAGS) -shared -o $(LIB_CLAVES) $(CLAVES_C) -lpthread
 
libproxyclaves.so: $(PROXY) $(CLAVES_H) $(PROTOCOLO_H)
	$(CC) $(CFLAGS) -shared -o $(LIB_PROXY) $(PROXY)
 
servidor: $(SERVIDOR) $(LIB_CLAVES)
	$(CC) $(CFLAGS) $(SERVIDOR) -o servidor ./libclaves.so -lpthread -Wl,-rpath,.
 
cliente: $(CLIENTE) $(LIB_PROXY)
	$(CC) $(CFLAGS) $(CLIENTE) -o cliente ./libproxyclaves.so -Wl,-rpath,.
 
clean:
	rm -f servidor cliente *.so *.o
