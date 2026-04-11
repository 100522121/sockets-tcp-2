/**
 * @file servidor-sock.c
 * @brief Servidor TCP concurrente del servicio de tuplas.
 *
 * Acepta conexiones TCP de clientes (proxies) y atiende cada una en un
 * hilo independiente usando el protocolo definido en protocolo.h, y acceso
 * concurrente a la lista de tuplas protegido con un mutex en claves.c.
 *
 * Uso: ./servidor <PUERTO>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "claves.h"
#include "protocolo.h"


/* ---- Helpers de E/S sobre el socket ---- */
/**
 * @brief Envía exactamente n bytes por el socket.
 * @return 0 en éxito, -1 en error.
 */
static int send_all(int fd, const char *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = write(fd, buf + sent, n - sent);
        if (r <= 0) return -1;
        sent += (size_t)r;
    }
    return 0;
}

/**
 * @brief Lee hasta encontrar '\n' o hasta MSG_MAX-1 bytes.
 *        El '\n' se sustituye por '\0'.
 * @return Número de bytes leídos (sin el '\n'), o -1 en error/cierre.
 */
static int recv_line(int fd, char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen - 1) {
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}


/* ---- Construcción de mensajes del protocolo ---- */
/**
 * @brief Serializa los floats y el Paquete en campos separados por '|'.
 *        Se añaden a la cadena @p out a partir de la posición actual.
 */
static void append_value2_value3(char *out, size_t out_size,
                                 int n, float *v, struct Paquete p) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%d", n);
    strncat(out, DELIM, out_size - strlen(out) - 1);
    strncat(out, tmp,   out_size - strlen(out) - 1);

    for (int i = 0; i < n; i++) {
        // Usar %.8g para preservar la precisión de los floats
        snprintf(tmp, sizeof(tmp), "%.8g", (double)v[i]);
        strncat(out, DELIM, out_size - strlen(out) - 1);
        strncat(out, tmp,   out_size - strlen(out) - 1);
    }

    /* x, y, z del Paquete */
    snprintf(tmp, sizeof(tmp), "%d", p.x);
    strncat(out, DELIM, out_size - strlen(out) - 1);
    strncat(out, tmp,   out_size - strlen(out) - 1);

    snprintf(tmp, sizeof(tmp), "%d", p.y);
    strncat(out, DELIM, out_size - strlen(out) - 1);
    strncat(out, tmp,   out_size - strlen(out) - 1);

    snprintf(tmp, sizeof(tmp), "%d", p.z);
    strncat(out, DELIM, out_size - strlen(out) - 1);
    strncat(out, tmp,   out_size - strlen(out) - 1);
                                 }


/* ---- Procesamiento de una petición ---- */
/**
* @brief Procesa un mensaje de petición y construye la respuesta.
*
* @param request  Cadena de texto con la petición (sin '\n').
* @param response Buffer donde se escribe la respuesta (sin '\n').
* @param resp_size Tamaño del buffer de respuesta.
*/
static void procesar_peticion(char *request, char *response, size_t resp_size) {
    // Trabajamos sobre una copia para no modificar el original
    char req_copy[MSG_MAX];
    strncpy(req_copy, request, MSG_MAX - 1);
    req_copy[MSG_MAX - 1] = '\0';

    // Extraer la operación solicitada por el cliente desde el mensaje
    char *tok;
    tok = strtok(req_copy, DELIM);
    if (tok == NULL) {
        strncpy(response, RESP_ERR, resp_size - 1);
        return;
    }

    // Comprobar si la operación solicitada corresponde a DESTROY
    if (strcmp(tok, OP_DESTROY) == 0) {   /* DESTROY */
        int r = destroy();
        snprintf(response, resp_size, "%s", r == 0 ? RESP_OK : RESP_ERR);
        return;
    }

    if (strcmp(tok, OP_EXIST) == 0) {     /* EXIST */
        char *key = strtok(NULL, DELIM);
        // Verificar que la clave recibida en la petición es válida
        if (key == NULL) { strncpy(response, RESP_ERR, resp_size-1); return; }
        // Consultar si la clave indicada existe en el almacenamiento del servidor
        int r = exist(key);
        if      (r == 1)  snprintf(response, resp_size, "OK|1");
        else if (r == 0)  snprintf(response, resp_size, "OK|0");
        else              snprintf(response, resp_size, "%s", RESP_ERR);
        return;
    }

    if (strcmp(tok, OP_DELETE) == 0) {    /* DELETE */
        char *key = strtok(NULL, DELIM);
        if (key == NULL) { strncpy(response, RESP_ERR, resp_size-1); return; }
        int r = delete_key(key);
        snprintf(response, resp_size, "%s", r == 0 ? RESP_OK : RESP_ERR);
        return;
    }

    if (strcmp(tok, OP_GET) == 0) {       /* GET */
        char *key = strtok(NULL, DELIM);
        if (key == NULL) { strncpy(response, RESP_ERR, resp_size-1); return; }

        char value1[256];
        int  N_value2;
        float V_value2[32];
        struct Paquete value3;

        // Recuperar los valores asociados a la clave solicitada mediante GET
        int r = get_value(key, value1, &N_value2, V_value2, &value3);
        if (r != 0) {
            snprintf(response, resp_size, "%s", RESP_ERR);
            return;
        }
        // Construir respuesta: OK|value1|N|f0|...|fN-1|x|y|z
        snprintf(response, resp_size, "OK|%s", value1);
        append_value2_value3(response, resp_size, N_value2, V_value2, value3);
        return;
    }

    if (strcmp(tok, OP_SET) == 0 ||       /* SET y MODIFY */
        strcmp(tok, OP_MODIFY) == 0) {

        // Extraer campos obligatorios de la petición SET/MODIFY recibida
        char *key    = strtok(NULL, DELIM);
        char *value1 = strtok(NULL, DELIM);
        char *n_str  = strtok(NULL, DELIM);

        // Determinar si la operación solicitada es MODIFY en lugar de SET
        int is_modify = (strcmp(tok, OP_MODIFY) == 0);
        int N_value2 = atoi(n_str);
        float V_value2[32];

        // Validar que estén los parámetros obligatorios de la petición
        if (key == NULL || value1 == NULL || n_str == NULL) {
            strncpy(response, RESP_ERR, resp_size-1); return;
        }
        if (N_value2 < 1 || N_value2 > 32) {
            strncpy(response, RESP_ERR, resp_size-1); return;
        }
        for (int i = 0; i < N_value2; i++) {
            char *f_str = strtok(NULL, DELIM);
            if (f_str == NULL) {
                strncpy(response, RESP_ERR, resp_size-1); return;
            }
            V_value2[i] = (float)atof(f_str);
        }

        // Extraer los valores x, y, z para la estructura Paquete
        char *x_str = strtok(NULL, DELIM);
        char *y_str = strtok(NULL, DELIM);
        char *z_str = strtok(NULL, DELIM);
        if (x_str == NULL || y_str == NULL || z_str == NULL) {
            strncpy(response, RESP_ERR, resp_size-1); return;
        }

        // Construir Paquete a partir de los valores recibidos
        struct Paquete value3;
        value3.x = atoi(x_str);
        value3.y = atoi(y_str);
        value3.z = atoi(z_str);

        // Ejecutar la operación SET o MODIFY correspondiente
        int r;
        if (is_modify)
            r = modify_value(key, value1, N_value2, V_value2, value3);
        else
            r = set_value(key, value1, N_value2, V_value2, value3);

        snprintf(response, resp_size, "%s", r == 0 ? RESP_OK : RESP_ERR);
        return;
    }
    // Operación desconocida
    strncpy(response, RESP_ERR, resp_size - 1);
}

/* ---- Hilo de atención a un cliente ---- */
/**
* @brief Atiende a un cliente. El hilo lee peticiones del socket, las
* procesa y devuelve la respuesta hasta que el cliente cierra la conexión.
*
* @param arg  Puntero a un int con el descriptor del socket del cliente.
*             La memoria apuntada debe ser liberada por este hilo.
*/
static void *atender_cliente(void *arg) {
    // Obtener el fd del socket del cliente pasado como argumento al hilo
    int client_fd = *((int *)arg);
    free(arg);

    // Buffers para almacenar la petición recibida y la respuesta generada
    char request[MSG_MAX];
    char response[MSG_MAX];

    // Hilo desprendido, por lo que no necesita ser unido por el padre
    pthread_detach(pthread_self());

    while (1) {
        int n = recv_line(client_fd, request, MSG_MAX);
        if (n <= 0) break;  // Cliente cerró la conexión o error
            procesar_peticion(request, response, MSG_MAX);

        // Añadir '\n' como terminador del mensaje de respuesta
        strncat(response, "\n", MSG_MAX - strlen(response) - 1);
        if (send_all(client_fd, response, strlen(response)) < 0) break;
    }
    close(client_fd);
    return NULL;
}

/* ---- main ---- */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    // Convertir el argumento en el puerto TCP en el que escuchará el servidor
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
        return 1;
    }

    // Crear un socket TCP IPv4 para aceptar conexiones entrantes de clientes
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Reutilizar puerto inmediatamente tras reinicio
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    // Configurar dirección del servidor para aceptar conexiones en
    // cualquier interfaz local
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    // Añadir comentario explicativo de una sola línea
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Asociar el socket servidor al puerto indicado
    if (listen(server_fd, 64) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("Servidor escuchando en puerto %d...\n", port);

    /* Bucle principal: aceptar conexiones y crear hilos */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Reservar memoria dinámica para almacenar el descriptor del cliente aceptado
        int *client_fd = malloc(sizeof(int));
        if (client_fd == NULL) {
            perror("malloc");
            continue;
        }

        // Aceptar una nueva conexión entrante desde un cliente proxy
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        // Crear un nuevo hilo para atender al cliente conectado
        pthread_t tid;
        if (pthread_create(&tid, NULL, atender_cliente, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        }
        /* El hilo se encarga de hacer detach y liberar client_fd */
    }
    close(server_fd);
    return 0;
}
