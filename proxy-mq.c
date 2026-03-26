#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>      /* O_CREAT, O_RDONLY, O_WRONLY */
#include <sys/stat.h>   /* permisos (0666) */
#include <stdatomic.h>  /* atomic_fetch_add para contador único */
#include "claves.h"

#define MAX_KEY  256
#define MAX_VAL1 256

/* Nombre de la cola del servidor (debe coincidir con servidor-mq.c) */
#define SERVER_QUEUE "/servidor_mq_prueba"

/* Códigos de operación (deben coincidir con servidor-mq.c) */
enum {
    OP_DESTROY = 0,
    OP_SET_VALUE,
    OP_GET_VALUE,
    OP_MODIFY_VALUE,
    OP_DELETE_KEY,
    OP_EXIST
};

/* Estructura de petición: Cliente -> Servidor */
struct Peticion {
    int  op;
    char q_nombre[256];       /* Cola de respuesta creada por el proxy */
    char key[MAX_KEY];
    char value1[MAX_VAL1];
    int  N_value2;
    float V_value2[32];
    struct Paquete value3;
};

/* Estructura de respuesta: Servidor -> Cliente */
struct Respuesta {
    int  resultado;
    char value1[MAX_VAL1];
    int  N_value2;
    float V_value2[32];
    struct Paquete value3;
};

/* Contador atómico global para generar nombres de cola únicos
 * incluso cuando varios hilos llaman al proxy simultáneamente. */
static atomic_int contador_cola = 0;


/**
 * realizar_peticion: función auxiliar interna.
 *
 * 1. Crea una cola de respuesta temporal exclusiva para
 *    esta llamada (nombre único: PID + contador atómico).
 * 2. Envía la petición a la cola del servidor.
 * 3. Espera la respuesta de forma bloqueante.
 * 4. Cierra y elimina la cola temporal.
 *
 * Devuelve el campo 'resultado' de la respuesta, o -2 si
 * se produce cualquier error en el sistema de mensajería.
 * --------------------------------------------------------- */
static int realizar_peticion(struct Peticion *pet, struct Respuesta *res) {
    mqd_t q_servidor, q_cliente;
    struct mq_attr attr;
    char nombre_cola_cliente[256];

    /* Generar un nombre único combinando PID y un contador atómico.
     * Esto garantiza unicidad tanto entre procesos como entre hilos. */
    int seq = atomic_fetch_add(&contador_cola, 1);
    snprintf(nombre_cola_cliente, sizeof(nombre_cola_cliente),
             "/proxy_%d_%d", (int)getpid(), seq);

    /* Guardar el nombre en la petición para que el servidor sepa
     * a qué cola enviar la respuesta */
    strncpy(pet->q_nombre, nombre_cola_cliente, 255);
    pet->q_nombre[255] = '\0';

    /* Atributos de la cola de respuesta: 1 mensaje del tamaño de Respuesta */
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = sizeof(struct Respuesta);
    attr.mq_curmsgs = 0;

    /* --- Paso 1: Crear la cola de respuesta temporal --- */
    mq_unlink(nombre_cola_cliente); /* Limpiar posible cola residual */
    q_cliente = mq_open(nombre_cola_cliente, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_cliente == (mqd_t)-1) {
        perror("proxy: error al crear cola de respuesta");
        return -2;
    }

    /* --- Paso 2: Abrir la cola del servidor --- */
    q_servidor = mq_open(SERVER_QUEUE, O_WRONLY);
    if (q_servidor == (mqd_t)-1) {
        perror("proxy: error al abrir cola del servidor (¿está arrancado?)");
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* --- Paso 3: Enviar la petición al servidor --- */
    if (mq_send(q_servidor, (const char *)pet, sizeof(struct Peticion), 0) == -1) {
        perror("proxy: error al enviar la petición");
        mq_close(q_servidor);
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* --- Paso 4: Esperar la respuesta del servidor --- */
    if (mq_receive(q_cliente, (char *)res, sizeof(struct Respuesta), NULL) == -1) {
        perror("proxy: error al recibir la respuesta");
        mq_close(q_servidor);
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2;
    }

    /* --- Paso 5: Limpieza de recursos --- */
    mq_close(q_servidor);
    mq_close(q_cliente);
    mq_unlink(nombre_cola_cliente);

    return res->resultado;
}

/** 
 * Implementación de la API pública (interfaz de claves.h)
 * Cada función empaqueta los parámetros en una Peticion,
 * la envía al servidor y devuelve el resultado.
 */

int destroy(void) {
    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_DESTROY;

    return realizar_peticion(&pet, &res);
}

int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    /* Validación local para evitar viaje innecesario al servidor */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (strlen(value1) > 255 || N_value2 > 32 || N_value2 < 1) return -1;

    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_SET_VALUE;
    strncpy(pet.key, key, MAX_KEY - 1);
    strncpy(pet.value1, value1, MAX_VAL1 - 1);
    pet.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        pet.V_value2[i] = V_value2[i];
    }
    pet.value3 = value3;

    return realizar_peticion(&pet, &res);
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    if (key == NULL || value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) return -1;

    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_GET_VALUE;
    strncpy(pet.key, key, MAX_KEY - 1);

    int ret = realizar_peticion(&pet, &res);
    if (ret == 0) {
        /* Copiar los valores de salida devueltos por el servidor */
        strcpy(value1, res.value1);
        *N_value2 = res.N_value2;
        for (int i = 0; i < res.N_value2; i++) {
            V_value2[i] = res.V_value2[i];
        }
        *value3 = res.value3;
    }
    return ret;
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    /* Validación local */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (strlen(value1) > 255 || N_value2 > 32 || N_value2 < 1) return -1;

    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_MODIFY_VALUE;
    strncpy(pet.key, key, MAX_KEY - 1);
    strncpy(pet.value1, value1, MAX_VAL1 - 1);
    pet.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        pet.V_value2[i] = V_value2[i];
    }
    pet.value3 = value3;

    return realizar_peticion(&pet, &res);
}

int delete_key(char *key) {
    if (key == NULL) return -1;

    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_DELETE_KEY;
    strncpy(pet.key, key, MAX_KEY - 1);

    return realizar_peticion(&pet, &res);
}

int exist(char *key) {
    if (key == NULL) return -1;

    struct Peticion pet;
    struct Respuesta res;
    memset(&pet, 0, sizeof(pet));

    pet.op = OP_EXIST;
    strncpy(pet.key, key, MAX_KEY - 1);

    return realizar_peticion(&pet, &res);
}
