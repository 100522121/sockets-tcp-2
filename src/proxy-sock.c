/**
 * @file proxy-sock.c
 * @brief Implementación de la biblioteca cliente del servicio de tuplas.
 *
 * Redirige cada llamada al servidor remoto mediante un socket TCP.
 * La IP y el puerto del servidor se obtienen de IP_TUPLAS y PORT_TUPLAS.
 * El protocolo de comunicación está definido en el fichero protocolo.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "claves.h"
#include "protocolo.h"


/* ---- Helpers de red ---- */
/**
 * @brief Abre una conexión TCP al servidor indicado por IP_TUPLAS y PORT_TUPLAS.
 * @return Descriptor de socket, o -1 si no se pudo conectar.
 */
static int conectar_servidor(void) {
    const char *ip_str   = getenv("IP_TUPLAS");
    const char *port_str = getenv("PORT_TUPLAS");

    // Comprobar que están definidas las variables de entorno para localizar el servidor
    if (ip_str == NULL || port_str == NULL) {
        fprintf(stderr, "[proxy] ERROR: variables IP_TUPLAS o PORT_TUPLAS no definidas\n");
        return -1;
    }

    // Convertir la cadena a entero y validar que esté dentro del rango válido TCP
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "[proxy] ERROR: PORT_TUPLAS='%s' no es válido\n", port_str);
        return -1;
    }

    // Resolver nombre de host (admite tanto IP decimal-punto como nombre)
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;        // Usar direcciones IPv4
    hints.ai_socktype = SOCK_STREAM;    // Usar sockets TCP orientados a conexión

    char port_buf[16];
    snprintf(port_buf, sizeof(port_buf), "%d", port);

    if (getaddrinfo(ip_str, port_buf, &hints, &res) != 0) {
        fprintf(stderr, "[proxy] ERROR: no se pudo resolver '%s'\n", ip_str);
        return -1;
    }

    // Crear un socket TCP IPv4 para la conexión con el servidor remoto
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[proxy] socket");
        freeaddrinfo(res);  // Liberar memoria reservada por getaddrinfo
        return -1;
    }

    // Establecer la conexión TCP con el servidor usando la dirección obtenida
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("[proxy] connect");
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

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
 * @brief Lee una línea terminada en '\n' del socket.
 *        El '\n' se sustituye por '\0'.
 * @return Bytes leídos (sin '\n'), o -1 en error/cierre.
 */
static int recv_line(int fd, char *buf, int maxlen) {
    int i = 0;
    char c;
    // Leer caracteres del socket hasta '\n' o llegar al tamaño máximo del buffer
    while (i < maxlen - 1) {
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';  // Finalizar añadiendo el end of string
    return i;
}



/*  ---- Enviar petición y obtener respuesta ---- */
/**
 * @brief Envía @p request al servidor y almacena la respuesta en @p response.
 *
 * @param request  Mensaje de petición sin '\n'.
 * @param response Buffer para la respuesta (sin '\n').
 * @param resp_size Tamaño del buffer.
 * @return 0 en éxito, -1 si el servidor devolvió ERR, -2 en error de comunicación.
 */
static int enviar_peticion(const char *request, char *response, size_t resp_size) {
    int fd = conectar_servidor();
    if (fd < 0) return -2;

    // Añadir '\n' al final del mensaje y enviarlo al servidor remoto
    char msg[MSG_MAX + 1];
    snprintf(msg, sizeof(msg), "%s\n", request);
    if (send_all(fd, msg, strlen(msg)) < 0) {
        close(fd);
        // No se pudo conectar o el servidor cerró la conexión inesperadamente
        return -2;
    }

    // Leer la respuesta del servidor
    if (recv_line(fd, response, (int)resp_size) < 0) {
        close(fd);
        return -2;
    }
    close(fd);

    // Interpretar código de respuesta
    if (strncmp(response, RESP_ERR, strlen(RESP_ERR)) == 0) return -1;
    return 0;  /* OK */
}


/*  ---- Construcción del mensaje SET/MODIFY ---- */
/**
 * @brief Construye el mensaje de petición para SET o MODIFY.
 */
static void construir_set_modify(char *buf, size_t buf_size, const char *op,
                                 char *key, char *value1, int N_value2,
                                 float *V_value2, struct Paquete value3) {
    // Construir la cabecera del mensaje con la operación, clave, value1 y N_value2
    char tmp[64];
    snprintf(buf, buf_size, "%s|%s|%s|%d", op, key, value1, N_value2);

    // Añadir al mensaje cada valor float delimitado del vector
    for (int i = 0; i < N_value2; i++) {
        snprintf(tmp, sizeof(tmp), "|%.8g", (double)V_value2[i]);
        strncat(buf, tmp, buf_size - strlen(buf) - 1);
    }

    // Añadir al final del mensaje los tres campos enteros de la struct value3
    snprintf(tmp, sizeof(tmp), "|%d|%d|%d", value3.x, value3.y, value3.z);
    strncat(buf, tmp, buf_size - strlen(buf) - 1);
}


/* ---- Implementación de la API (proxy remoto) ---- */
int destroy(void) {
    char response[MSG_MAX];
    int r = enviar_peticion(OP_DESTROY, response, MSG_MAX);
    if (r == -2) return -2;
    return r;  // 0 o -1 
}

int set_value(char *key, char *value1, int N_value2,
              float *V_value2, struct Paquete value3) {
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32) return -1;

    char request[MSG_MAX];
    construir_set_modify(request, MSG_MAX, OP_SET,
                          key, value1, N_value2, V_value2, value3);

    char response[MSG_MAX];
    int r = enviar_peticion(request, response, MSG_MAX);
    if (r == -2) return -2;
    return r;
}

int get_value(char *key, char *value1, int *N_value2,
              float *V_value2, struct Paquete *value3) {
    if (key == NULL || value1 == NULL || N_value2 == NULL ||
        V_value2 == NULL || value3 == NULL) return -1;

    char request[MSG_MAX];
    snprintf(request, MSG_MAX, "%s|%s", OP_GET, key);

    char response[MSG_MAX];
    int r = enviar_peticion(request, response, MSG_MAX);
    if (r != 0) return r;  // -1 (clave no existe) o -2 (error red)

    /* Parsear: OK|value1|N|f0|...|fN-1|x|y|z */
    char resp_copy[MSG_MAX];
    strncpy(resp_copy, response, MSG_MAX - 1);

    /* Saltar "OK" */
    char *tok = strtok(resp_copy, DELIM);  /* "OK" */
    if (tok == NULL) return -2;

    tok = strtok(NULL, DELIM);             /* value 1 */
    if (tok == NULL) return -2;
    strncpy(value1, tok, 255);
    value1[255] = '\0';  // Terminación segura en el buffer destino

    tok = strtok(NULL, DELIM);             /* N_value2 */
    if (tok == NULL) return -2;
    *N_value2 = atoi(tok);
    // Convertir cada valor float recibido en el vector V_value2
    for (int i = 0; i < *N_value2; i++) {
        tok = strtok(NULL, DELIM);
        if (tok == NULL) return -2;
        V_value2[i] = (float)atof(tok);
    }

    tok = strtok(NULL, DELIM);             /* x */
    if (tok == NULL) return -2;
    value3->x = atoi(tok);

    tok = strtok(NULL, DELIM);             /* y */
    if (tok == NULL) return -2;
    value3->y = atoi(tok);

    tok = strtok(NULL, DELIM);             /* z */
    if (tok == NULL) return -2;
    value3->z = atoi(tok);

    return 0;
}

int modify_value(char *key, char *value1, int N_value2,
                 float *V_value2, struct Paquete value3) {
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32) return -1;

    char request[MSG_MAX];
    // Construir la petición MODIFY siguiendo el mismo formato que SET
    construir_set_modify(request, MSG_MAX, OP_MODIFY,
                          key, value1, N_value2, V_value2, value3);

    char response[MSG_MAX];
    int r = enviar_peticion(request, response, MSG_MAX);
    if (r == -2) return -2;
    return r;
}

int delete_key(char *key) {
    if (key == NULL) return -1;

    // Construir la petición DELETE incluyendo la operación y la clave
    char request[MSG_MAX];
    snprintf(request, MSG_MAX, "%s|%s", OP_DELETE, key);

    char response[MSG_MAX];
    int r = enviar_peticion(request, response, MSG_MAX);
    if (r == -2) return -2;
    return r;
}

int exist(char *key) {
    if (key == NULL) return -1;

    // Construir la petición EXIST para consultar si la clave está almacenada
    char request[MSG_MAX];
    snprintf(request, MSG_MAX, "%s|%s", OP_EXIST, key);

    char response[MSG_MAX];
    int r = enviar_peticion(request, response, MSG_MAX);
    if (r == -2) return -2;
    if (r == -1) return -1;

    /* Parsear: OK|0 o OK|1 */
    char resp_copy[MSG_MAX];
    strncpy(resp_copy, response, MSG_MAX - 1);
    strtok(resp_copy, DELIM);           // "OK"
    char *val = strtok(NULL, DELIM);    // "0" o "1"
    if (val == NULL) return -2;
    return atoi(val);
}
