#ifndef PROTOCOLO_MQ_H
#define PROTOCOLO_MQ_H

/**
 * @file protocolo-mq.h
 * @brief Protocolo de comunicación entre proxy y servidor mediante
 *        colas de mensajes POSIX.
 *
 * Este fichero centraliza en un único lugar las definiciones que
 * deben ser idénticas en ambos extremos de la comunicación:
 *  - Nombre de la cola del servidor.
 *  - Códigos de operación.
 *  - Estructuras de petición y respuesta.
 *
 * Incluirlo en proxy-mq.c Y en servidor-mq.c garantiza que cualquier
 * cambio en el protocolo se aplica de forma consistente.
 */

#include "claves.h"   /* struct Paquete */

/* ------------------------------------------------------------------ */
/*  Parámetros de la cola del servidor                                  */
/* ------------------------------------------------------------------ */

/** Nombre POSIX de la cola del servidor.  Debe comenzar por '/'. */
#define SERVER_QUEUE     "/servidor_mq_prueba"

/** Capacidad máxima de mensajes en las colas (servidor y respuesta). */
#define MQ_MAX_MSG       10

/* ------------------------------------------------------------------ */
/*  Límites de los campos de las tuplas                                 */
/* ------------------------------------------------------------------ */

#define MAX_KEY          256   /**< Bytes máximos de la clave (con '\0'). */
#define MAX_VAL1         256   /**< Bytes máximos de value1 (con '\0').   */
#define MAX_VEC_SIZE     32    /**< Número máximo de floats en value2.    */

/* ------------------------------------------------------------------ */
/*  Códigos de operación                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Identificadores numéricos de las operaciones de la API.
 *
 * El proxy escribe el código en Peticion.op para que el servidor sepa
 * qué función de libclaves.so debe invocar.
 */
typedef enum {
    OP_DESTROY      = 0,  /**< destroy()       */
    OP_SET_VALUE    = 1,  /**< set_value()      */
    OP_GET_VALUE    = 2,  /**< get_value()      */
    OP_MODIFY_VALUE = 3,  /**< modify_value()   */
    OP_DELETE_KEY   = 4,  /**< delete_key()     */
    OP_EXIST        = 5   /**< exist()          */
} CodigoOp;

/* ------------------------------------------------------------------ */
/*  Estructuras de mensajes                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Mensaje enviado por el proxy al servidor.
 *
 * Contiene el código de operación, el nombre de la cola de respuesta
 * temporal que el proxy ha creado, y todos los parámetros de entrada
 * posibles de la API.  Los campos irrelevantes para una operación
 * concreta se ignoran en el servidor.
 */
struct Peticion {
    CodigoOp op;               /**< Operación solicitada.                */
    char q_nombre[MAX_KEY];    /**< Cola POSIX de respuesta del proxy.   */

    /* Parámetros de entrada de la API */
    char  key[MAX_KEY];
    char  value1[MAX_VAL1];
    int   N_value2;
    float V_value2[MAX_VEC_SIZE];
    struct Paquete value3;
};

/**
 * @brief Mensaje enviado por el servidor al proxy como respuesta.
 *
 * El campo resultado transporta el valor de retorno de la función
 * ejecutada.  Los demás campos solo contienen datos válidos cuando
 * la operación es OP_GET_VALUE y resultado == 0.
 */
struct Respuesta {
    int  resultado;              /**< Valor de retorno de la función.      */

    /* Parámetros de salida (solo válidos para OP_GET_VALUE con éxito) */
    char  value1[MAX_VAL1];
    int   N_value2;
    float V_value2[MAX_VEC_SIZE];
    struct Paquete value3;
};

#endif /* PROTOCOLO_MQ_H */
