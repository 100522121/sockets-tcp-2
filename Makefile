# ═══════════════════════════════════════════════════════════════════
# Makefile — Servicio de Tuplas con Colas de Mensajes POSIX
# ═══════════════════════════════════════════════════════════════════
#
# Estructura de directorios:
#   include/   → cabeceras públicas (claves.h, protocolo-mq.h)
#   src/       → código fuente (claves.c, proxy-mq.c, servidor-mq.c)
#   tests/     → cliente de pruebas (app-cliente.c)
#   build/     → objetos intermedios (.o) generados automáticamente
#   .          → bibliotecas y ejecutables finales
#
# Targets principales:
#   make            → compila todo
#   make parte-a    → solo versión no distribuida
#   make parte-b    → solo versión distribuida
#   make clean      → elimina todos los ficheros generados

# ───────────────────────────────────────────────────────────────────
# Variables de compilación
# ───────────────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -fPIC -g -Iinclude
LDFLAGS = -shared
LDLIBS  = -lpthread -lrt

# Directorio de objetos intermedios
BUILDDIR = build

# ───────────────────────────────────────────────────────────────────
# Artefactos de salida
# ───────────────────────────────────────────────────────────────────

LIB_CLAVES  = libclaves.so
LIB_PROXY   = libproxyclaves.so
SERVER      = servidor_mq
CLIENT_A    = app-cliente        # Parte A: enlaza con libclaves.so
CLIENT_B    = app-cliente-mq     # Parte B: enlaza con libproxyclaves.so

# ───────────────────────────────────────────────────────────────────
# Target por defecto: construir todo
# ───────────────────────────────────────────────────────────────────

.PHONY: all parte-a parte-b clean

all: $(LIB_CLAVES) $(LIB_PROXY) $(SERVER) $(CLIENT_A) $(CLIENT_B)

# ───────────────────────────────────────────────────────────────────
# Crear el directorio de build si no existe
# ───────────────────────────────────────────────────────────────────

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ───────────────────────────────────────────────────────────────────
# PARTE A — Versión no distribuida
# ───────────────────────────────────────────────────────────────────

# Objeto de la biblioteca de claves
$(BUILDDIR)/claves.o: src/claves.c include/claves.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Biblioteca dinámica libclaves.so
$(LIB_CLAVES): $(BUILDDIR)/claves.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Cliente no distribuido: enlaza directamente con libclaves.so
$(CLIENT_A): tests/app-cliente.c include/claves.h $(LIB_CLAVES)
	$(CC) $(CFLAGS) -o $@ $< -L. -lclaves -Wl,-rpath,. $(LDLIBS)

parte-a: $(LIB_CLAVES) $(CLIENT_A)

# ───────────────────────────────────────────────────────────────────
# PARTE B — Versión distribuida con colas POSIX
# ───────────────────────────────────────────────────────────────────

# Objeto del proxy
$(BUILDDIR)/proxy-mq.o: src/proxy-mq.c include/claves.h include/protocolo-mq.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Biblioteca dinámica libproxyclaves.so
$(LIB_PROXY): $(BUILDDIR)/proxy-mq.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Servidor: enlaza con libclaves.so para ejecutar las operaciones
$(SERVER): src/servidor-mq.c include/claves.h include/protocolo-mq.h $(LIB_CLAVES)
	$(CC) $(CFLAGS) -o $@ $< -L. -lclaves -Wl,-rpath,. $(LDLIBS)

# Cliente distribuido: enlaza con libproxyclaves.so (misma fuente que CLIENT_A)
$(CLIENT_B): tests/app-cliente.c include/claves.h $(LIB_PROXY)
	$(CC) $(CFLAGS) -o $@ $< -L. -lproxyclaves -Wl,-rpath,. $(LDLIBS)

parte-b: $(LIB_PROXY) $(SERVER) $(CLIENT_B)

# ───────────────────────────────────────────────────────────────────
# Limpieza
# ───────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILDDIR) *.so $(CLIENT_A) $(CLIENT_B) $(SERVER)
