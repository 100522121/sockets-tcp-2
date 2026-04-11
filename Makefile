# Variables de compilación
CC = gcc
CFLAGS = -Wall -fPIC -g

# Artefactos de salida
LIB_CLAVES  = libclaves.so
LIB_PROXY 	= libproxyclaves.so  
PROXY 		= proxy-sock.c
SERVIDOR	= servidor-sock.c 
CLIENTE 	= app-cliente.c 

# Crear el directorio de build si no existe
build: 
	mkdir -p $(BUILDDIR)

all: servidor cliente $(LIB_CLAVES) $(LIB_PROXY)
 
libclaves.so: claves.c claves.h
	$(CC) $(CFLAGS) -shared -o $(LIB_CLAVES) claves.c -lpthread
 
libproxyclaves.so: $(PROXY) claves.h protocolo.h
	$(CC) $(CFLAGS) -shared -o $(LIB_PROXY) $(PROXY)
 
servidor: servidor-sock.c $(LIB_CLAVES)
	$(CC) $(CFLAGS) $(SERVIDOR) -o servidor ./libclaves.so -lpthread -Wl,-rpath,.
 
cliente: app-cliente.c $(LIB_PROXY)
	$(CC) $(CFLAGS) $(CLIENTE) -o cliente ./libproxyclaves.so -Wl,-rpath,.
 
clean:
	rm -f servidor cliente *.so *.o
