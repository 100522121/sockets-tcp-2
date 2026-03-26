/**
 * @file servidor-mq.c
 * @brief Servidor concurrente del servicio de tuplas (POSIX MQ).
 *
 * Arquitectura:
 *  - Bucle principal: recibe peticiones en la cola SERVER_QUEUE y
 *    lanza un hilo POSIX detached por cada una.
 *  - Hilo de atención: invoca la función correspondiente de
 *    libclaves.so y envía la respuesta a la cola temporal del proxy.
 *
 * La concurrencia es segura porque claves.c serializa todos los
 * accesos a la lista enlazada mediante un mutex interno.
 *
 * Uso:
 *   ./servidor_mq
 *   (El servidor no acepta argumentos; se detiene con Ctrl-C.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "claves.h"
#include "protocolo-mq.h"


/* ------------------------------------------------------------------ */
/*  Función ejecutada por cada hilo de atención                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Atiende una única petición de cliente en un hilo independiente.
 *
 * El puntero @p arg apunta a una struct Peticion asignada con malloc()
 * en el bucle principal.  Este hilo toma posesión de ese bloque y lo
 * libera con free() antes de terminar.
 *
 * Flujo:
 *  1. Copiar la petición al stack y liberar la memoria dinámica.
 *  2. Despachar la operación invocando la función de libclaves.so.
 *  3. Abrir la cola de respuesta indicada en pet.q_nombre.
 *  4. Enviar la respuesta; controlar el error de mq_send.
 *  5. Cerrar la cola de respuesta.
 *
 * @param arg Puntero a struct Peticion* (propiedad transferida).
 * @return NULL (hilo detached; el valor de retorno no se usa).
 */
void *atender_peticion(void *arg) {
    /* Tomar posesión del paquete y copiar al stack */
    struct Peticion pet = *(struct Peticion *)arg;
    free(arg);  /* Liberar la memoria asignada en el bucle principal */

    struct Respuesta res;
    memset(&res, 0, sizeof(res));

    /* --- Despachar la operación solicitada ------------------------ */
    switch (pet.op) {

        case OP_DESTROY:
            res.resultado = destroy();
            break;

        case OP_SET_VALUE:
            res.resultado = set_value(pet.key, pet.value1,
                                      pet.N_value2, pet.V_value2,
                                      pet.value3);
            break;

        case OP_GET_VALUE:
            /* Los parámetros de salida se escriben directamente en res
             * para evitar copias intermedias */
            res.resultado = get_value(pet.key, res.value1,
                                      &res.N_value2, res.V_value2,
                                      &res.value3);
            break;

        case OP_MODIFY_VALUE:
            res.resultado = modify_value(pet.key, pet.value1,
                                         pet.N_value2, pet.V_value2,
                                         pet.value3);
            break;

        case OP_DELETE_KEY:
            res.resultado = delete_key(pet.key);
            break;

        case OP_EXIST:
            res.resultado = exist(pet.key);
            break;

        default:
            /* Código de operación desconocido */
            fprintf(stderr, "servidor: código de operación desconocido: %d\n",
                    pet.op);
            res.resultado = -1;
            break;
    }

    /* --- Enviar la respuesta al proxy ----------------------------- */
    mqd_t q_cliente = mq_open(pet.q_nombre, O_WRONLY);
    if (q_cliente == (mqd_t)-1) {
        perror("servidor: error al abrir la cola de respuesta del cliente");
        pthread_exit(NULL);
    }

    /* Controlar el error de mq_send: si falla, el proxy recibirá
     * un timeout o un error de mq_receive, pero al menos lo registramos */
    if (mq_send(q_cliente, (const char *)&res, sizeof(res), 0) == -1) {
        perror("servidor: error al enviar la respuesta al cliente");
    }

    mq_close(q_cliente);
    pthread_exit(NULL);
}


/* ------------------------------------------------------------------ */
/*  Bucle principal                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Punto de entrada del servidor.
 *
 * 1. Elimina una posible cola residual de ejecuciones anteriores.
 * 2. Crea la cola del servidor con los atributos apropiados.
 * 3. Entra en un bucle infinito que:
 *    a. Espera de forma bloqueante la siguiente petición.
 *    b. Asigna memoria para la petición.
 *    c. Lanza un hilo detached que la atiende.
 *
 * El servidor no tiene mecanismo de parada ordenada; debe detenerse
 * con SIGINT (Ctrl-C) o SIGTERM.
 */
int main(void) {
    mqd_t q_servidor;
    struct mq_attr attr;

    /* Atributos de la cola: tamaño de mensaje = sizeof(Peticion) */
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MQ_MAX_MSG;
    attr.mq_msgsize = sizeof(struct Peticion);
    attr.mq_curmsgs = 0;

    /* Eliminar la cola si quedó de una ejecución anterior */
    mq_unlink(SERVER_QUEUE);

    q_servidor = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_servidor == (mqd_t)-1) {
        perror("servidor: error al crear la cola del servidor");
        return -1;
    }

    printf("Servidor de Tuplas (POSIX MQ) iniciado.\n");
    printf("Escuchando en la cola: %s\n", SERVER_QUEUE);
    printf("Pulse Ctrl-C para detener.\n\n");

    /* ---- Bucle principal de aceptación de peticiones ------------- */
    while (1) {

        /* Reservar espacio para la petición entrante.
         * La memoria se transfiere al hilo de atención que la libera. */
        struct Peticion *pet = malloc(sizeof(struct Peticion));
        if (pet == NULL) {
            perror("servidor: error al reservar memoria para la petición");
            continue;   /* Intentar con la siguiente petición */
        }

        /* Bloquear hasta recibir la siguiente petición */
        if (mq_receive(q_servidor, (char *)pet,
                       sizeof(struct Peticion), NULL) == -1) {
            perror("servidor: error en mq_receive");
            free(pet);
            continue;
        }

        /* Lanzar un hilo para atender la petición concurrentemente */
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atender_peticion, pet) != 0) {
            perror("servidor: error al crear el hilo de atención");
            free(pet);  /* Liberar aquí porque el hilo no lo hará */
        } else {
            /* Detach: el hilo libera sus recursos al terminar,
             * sin necesidad de que nadie haga join */
            pthread_detach(hilo);
        }
    }

    /* Código inalcanzable en operación normal */
    mq_close(q_servidor);
    mq_unlink(SERVER_QUEUE);
    return 0;
}
