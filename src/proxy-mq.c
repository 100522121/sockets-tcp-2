/**
 * @file proxy-mq.c
 * @brief Biblioteca cliente distribuida (libproxyclaves.so).
 *
 * Implementa la API definida en claves.h de forma distribuida:
 * cada llamada a la API se transforma en un mensaje enviado al
 * servidor mediante colas de mensajes POSIX y la respuesta se
 * recibe por una cola temporal exclusiva de esta llamada.
 *
 * Convención de códigos de retorno:
 *   0   → operación completada con éxito.
 *  -1   → error lógico del servicio (clave inexistente, duplicada…).
 *  -2   → error en el sistema de comunicaciones (cola inexistente,
 *          fallo de mq_send/mq_receive, etc.).
 *
 * Seguridad en hilos:
 *   realizar_peticion() es reentrante.  El nombre de la cola de
 *   respuesta se genera combinando el PID del proceso con un contador
 *   atómico, garantizando unicidad incluso cuando varios hilos
 *   invocan la API de forma simultánea.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "claves.h"
#include "protocolo-mq.h"

/* ------------------------------------------------------------------ */
/*  Estado global del módulo                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Contador atómico usado para construir nombres de cola únicos.
 *
 * Se incrementa con atomic_fetch_add, operación libre de condiciones
 * de carrera incluso con múltiples hilos concurrentes.
 */
static atomic_int contador_cola = 0;


/* ------------------------------------------------------------------ */
/*  Función auxiliar interna                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Envía una petición al servidor y espera su respuesta.
 *
 * Pasos que realiza esta función:
 *  1. Genera un nombre de cola único (/proxy_<PID>_<seq>).
 *  2. Crea la cola de respuesta temporal con ese nombre.
 *  3. Abre la cola del servidor (debe estar ya en ejecución).
 *  4. Envía la petición; controla el error de mq_send.
 *  5. Espera la respuesta de forma bloqueante.
 *  6. Cierra y elimina la cola temporal.
 *
 * En cualquier paso en que se produzca un error del sistema de
 * mensajería se libera todo recurso abierto y se devuelve -2.
 *
 * @param[in,out] pet  Petición a enviar.  El campo q_nombre se
 *                     rellena aquí antes del envío.
 * @param[out]    res  Buffer donde se almacena la respuesta recibida.
 * @return El campo resultado de la respuesta, o -2 ante error de IPC.
 */
static int realizar_peticion(struct Peticion *pet, struct Respuesta *res) {
    mqd_t q_servidor = (mqd_t)-1;
    mqd_t q_cliente  = (mqd_t)-1;
    struct mq_attr attr;
    char nombre_cola_cliente[256];

    /* --- Paso 1: Generar nombre de cola único ---------------------- */
    int seq = atomic_fetch_add(&contador_cola, 1);
    snprintf(nombre_cola_cliente, sizeof(nombre_cola_cliente),
             "/proxy_%d_%d", (int)getpid(), seq);

    /* Guardar el nombre en la petición para que el servidor sepa
     * a dónde enviar la respuesta */
    strncpy(pet->q_nombre, nombre_cola_cliente, MAX_KEY - 1);
    pet->q_nombre[MAX_KEY - 1] = '\0';

    /* --- Paso 2: Crear la cola de respuesta temporal -------------- */
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MQ_MAX_MSG;
    attr.mq_msgsize = sizeof(struct Respuesta);
    attr.mq_curmsgs = 0;

    mq_unlink(nombre_cola_cliente); /* Eliminar posible cola residual */
    q_cliente = mq_open(nombre_cola_cliente, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_cliente == (mqd_t)-1) {
        perror("proxy: error al crear la cola de respuesta");
        return -2;
    }

    /* --- Paso 3: Abrir la cola del servidor ----------------------- */
    q_servidor = mq_open(SERVER_QUEUE, O_WRONLY);
    if (q_servidor == (mqd_t)-1) {
        perror("proxy: error al abrir la cola del servidor "
               "(¿está el servidor en ejecución?)");
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* --- Paso 4: Enviar la petición ------------------------------- */
    if (mq_send(q_servidor, (const char *)pet,
                sizeof(struct Peticion), 0) == -1) {
        perror("proxy: error al enviar la petición al servidor");
        mq_close(q_servidor);
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* Ya no necesitamos la cola de escritura del servidor */
    mq_close(q_servidor);
    q_servidor = (mqd_t)-1;

    /* --- Paso 5: Esperar la respuesta ----------------------------- */
    if (mq_receive(q_cliente, (char *)res,
                   sizeof(struct Respuesta), NULL) == -1) {
        perror("proxy: error al recibir la respuesta del servidor");
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* --- Paso 6: Limpiar la cola temporal ------------------------- */
    mq_close(q_cliente);
    mq_unlink(nombre_cola_cliente);

    return res->resultado;
}


/* ------------------------------------------------------------------ */
/*  Implementación de la API pública                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Versión distribuida de destroy().
 *
 * Empaqueta la operación OP_DESTROY y la delega en el servidor.
 * No requiere parámetros adicionales.
 */
int destroy(void) {
    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_DESTROY;

    return realizar_peticion(&pet, &res);
}

/**
 * @brief Versión distribuida de set_value().
 *
 * Realiza validaciones locales (parámetros NULL, rangos) antes de
 * contactar con el servidor, evitando un viaje innecesario.
 * Devuelve -1 si los parámetros son inválidos, -2 si hay error IPC.
 */
int set_value(char *key, char *value1, int N_value2,
              float *V_value2, struct Paquete value3) {

    /* Validación local: evita el viaje al servidor en casos obvios */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (strlen(value1) > 255 || N_value2 > 32 || N_value2 < 1)  return -1;

    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_SET_VALUE;
    strncpy(pet.key,    key,    MAX_KEY  - 1);
    strncpy(pet.value1, value1, MAX_VAL1 - 1);
    pet.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        pet.V_value2[i] = V_value2[i];
    }
    pet.value3 = value3;

    return realizar_peticion(&pet, &res);
}

/**
 * @brief Versión distribuida de get_value().
 *
 * Si el servidor devuelve éxito (resultado == 0), copia los valores
 * de salida contenidos en la Respuesta a los buffers del llamador.
 */
int get_value(char *key, char *value1, int *N_value2,
              float *V_value2, struct Paquete *value3) {

    if (key == NULL || value1 == NULL || N_value2 == NULL ||
        V_value2 == NULL || value3 == NULL) return -1;

    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_GET_VALUE;
    strncpy(pet.key, key, MAX_KEY - 1);

    int ret = realizar_peticion(&pet, &res);
    if (ret == 0) {
        /* Copiar los valores de salida de la respuesta a los buffers
         * del cliente */
        strcpy(value1, res.value1);
        *N_value2 = res.N_value2;
        for (int i = 0; i < res.N_value2; i++) {
            V_value2[i] = res.V_value2[i];
        }
        *value3 = res.value3;
    }
    return ret;
}

/**
 * @brief Versión distribuida de modify_value().
 *
 * Idéntica a set_value en cuanto a validación y empaquetado,
 * pero usa el código de operación OP_MODIFY_VALUE.
 */
int modify_value(char *key, char *value1, int N_value2,
                 float *V_value2, struct Paquete value3) {

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (strlen(value1) > 255 || N_value2 > 32 || N_value2 < 1)  return -1;

    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_MODIFY_VALUE;
    strncpy(pet.key,    key,    MAX_KEY  - 1);
    strncpy(pet.value1, value1, MAX_VAL1 - 1);
    pet.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        pet.V_value2[i] = V_value2[i];
    }
    pet.value3 = value3;

    return realizar_peticion(&pet, &res);
}

/**
 * @brief Versión distribuida de delete_key().
 */
int delete_key(char *key) {
    if (key == NULL) return -1;

    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_DELETE_KEY;
    strncpy(pet.key, key, MAX_KEY - 1);

    return realizar_peticion(&pet, &res);
}

/**
 * @brief Versión distribuida de exist().
 */
int exist(char *key) {
    if (key == NULL) return -1;

    struct Peticion  pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_EXIST;
    strncpy(pet.key, key, MAX_KEY - 1);

    return realizar_peticion(&pet, &res);
}
