CC      = gcc
CFLAGS  = -Wall -Wextra -fPIC -g
LDFLAGS = -shared
LDLIBS  = -lpthread -lrt
 
# ─── Archivos de salida ───────────────────────────────────────────
LIB_CLAVES  = libclaves.so
LIB_PROXY   = libproxyclaves.so
SERVER      = servidor_mq
CLIENT_A    = app-cliente       # Parte A: sin colas (libclaves.so)
CLIENT_B    = app-cliente-mq    # Parte B: con colas (libproxyclaves.so)
 
# ─── Objetivo por defecto: construye todo ────────────────────────
all: $(LIB_CLAVES) $(LIB_PROXY) $(SERVER) $(CLIENT_A) $(CLIENT_B)
 
# ─── PARTE A — Versión no distribuida ────────────────────────────
$(LIB_CLAVES): claves.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
 
claves.o: claves.c claves.h
	$(CC) $(CFLAGS) -c $<
 
$(CLIENT_A): app-cliente.c claves.h $(LIB_CLAVES)
	$(CC) -Wall -Wextra -g -o $@ $< -L. -lclaves -Wl,-rpath,. $(LDLIBS)
 
# ─── PARTE B — Versión distribuida con colas POSIX ───────────────
$(LIB_PROXY): proxy-mq.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
 
proxy-mq.o: proxy-mq.c claves.h
	$(CC) $(CFLAGS) -c $<
 
$(SERVER): servidor-mq.c claves.h $(LIB_CLAVES)
	$(CC) -Wall -Wextra -g -o $@ servidor-mq.c -L. -lclaves -Wl,-rpath,. $(LDLIBS)
 
$(CLIENT_B): app-cliente.c claves.h $(LIB_PROXY)
	$(CC) -Wall -Wextra -g -o $@ $< -L. -lproxyclaves -Wl,-rpath,. $(LDLIBS)
 
# ─── Targets de utilidad ─────────────────────────────────────────
parte-a: $(LIB_CLAVES) $(CLIENT_A)
parte-b: $(LIB_PROXY) $(SERVER) $(CLIENT_B)
 
clean:
	rm -f *.o *.so $(CLIENT_A) $(CLIENT_B) $(SERVER)
 
.PHONY: all parte-a parte-b clean