#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>      /* O_CREAT, O_RDONLY, O_WRONLY */
#include <sys/stat.h>   /* permisos de la cola (0666) */
#include "claves.h"

#define MAX_KEY  256
#define MAX_VAL1 256

/* Nombre de la cola del servidor (debe coincidir con proxy) */
#define SERVER_QUEUE "/servidor_mq_prueba"

/* Códigos de operación (deben coincidir con proxy-mq.c) */
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
    int  op;                  /* Código de operación */
    char q_nombre[256];       /* Nombre de la cola de respuesta del cliente */
    char key[MAX_KEY];
    char value1[MAX_VAL1];
    int  N_value2;
    float V_value2[32];
    struct Paquete value3;
};

/* Estructura de respuesta: Servidor -> Cliente */
struct Respuesta {
    int  resultado;           /* Valor de retorno de la función */
    char value1[MAX_VAL1];    /* Solo usado en get_value */
    int  N_value2;            /* Solo usado en get_value */
    float V_value2[32];       /* Solo usado en get_value */
    struct Paquete value3;    /* Solo usado en get_value */
};


/**
 * atender_peticion: función ejecutada por cada hilo.
 *
 * Recibe la petición ya copiada en memoria dinámica,
 * invoca la función de la biblioteca libclaves.so
 * correspondiente, y envía la respuesta al cliente.
 */
void *atender_peticion(void *arg) {
    /* Tomar posesión del paquete y liberar al salir */
    struct Peticion pet = *(struct Peticion *)arg;
    free(arg);

    struct Respuesta res;
    memset(&res, 0, sizeof(struct Respuesta));

    /* Despachar la operación solicitada */
    switch (pet.op) {
        case OP_DESTROY:
            res.resultado = destroy();
            break;
        case OP_SET_VALUE:
            res.resultado = set_value(pet.key, pet.value1,
                                      pet.N_value2, pet.V_value2, pet.value3);
            break;
        case OP_GET_VALUE:
            /* Los valores de salida se escriben directamente en res */
            res.resultado = get_value(pet.key, res.value1,
                                      &res.N_value2, res.V_value2, &res.value3);
            break;
        case OP_MODIFY_VALUE:
            res.resultado = modify_value(pet.key, pet.value1,
                                         pet.N_value2, pet.V_value2, pet.value3);
            break;
        case OP_DELETE_KEY:
            res.resultado = delete_key(pet.key);
            break;
        case OP_EXIST:
            res.resultado = exist(pet.key);
            break;
        default:
            res.resultado = -1;
            break;
    }

    /* Abrir la cola de respuesta del cliente y enviar el resultado */
    mqd_t q_cliente = mq_open(pet.q_nombre, O_WRONLY);
    if (q_cliente != (mqd_t)-1) {
        mq_send(q_cliente, (const char *)&res, sizeof(res), 0);
        mq_close(q_cliente);
    } else {
        perror("Error al abrir cola de respuesta del cliente");
    }

    pthread_exit(NULL);
}


/** 
 * main: bucle principal del servidor.
 *
 * Crea la cola del servidor, espera peticiones y lanza
 * un hilo independiente (detached) por cada una.
 */
int main(void) {
    mqd_t q_servidor;
    struct mq_attr attr;

    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = sizeof(struct Peticion);
    attr.mq_curmsgs = 0;

    /* Eliminar cola residual de ejecuciones anteriores */
    mq_unlink(SERVER_QUEUE);

    q_servidor = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_servidor == (mqd_t)-1) {
        perror("Error al crear la cola del servidor");
        return -1;
    }

    printf("Servidor de Tuplas (POSIX MQ) iniciado y escuchando en %s...\n", SERVER_QUEUE);

    /* Bucle de aceptación de peticiones */
    while (1) {
        struct Peticion *pet = malloc(sizeof(struct Peticion));
        if (pet == NULL) {
            perror("Error al reservar memoria para la petición");
            continue;
        }

        /* Esperar de forma bloqueante la siguiente petición */
        if (mq_receive(q_servidor, (char *)pet, sizeof(struct Peticion), NULL) == -1) {
            perror("Error en mq_receive");
            free(pet);
            continue;
        }

        /* Lanzar un hilo que atienda la petición de forma concurrente */
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atender_peticion, (void *)pet) != 0) {
            perror("Error al crear el hilo de atención");
            free(pet);
        } else {
            /* Detach: el hilo libera sus recursos al terminar sin join */
            pthread_detach(hilo);
        }
    }

    /* Nunca se alcanza en condiciones normales */
    mq_close(q_servidor);
    mq_unlink(SERVER_QUEUE);
    return 0;
}
