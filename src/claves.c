/**
 * @file claves.c
 * @brief Implementación del servicio de almacenamiento de tuplas
 *        <key, value1, value2, value3>.
 *
 * Las tuplas se almacenan en una lista enlazada simple con asignación
 * dinámica de memoria, lo que no impone ningún límite en el número de
 * elementos que pueden almacenarse (salvo la memoria disponible del
 * sistema).
 *
 * Todas las operaciones adquieren el mismo mutex antes de acceder a
 * la lista, garantizando así la atomicidad requerida cuando varios
 * hilos del servidor invocan estas funciones de forma concurrente.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "claves.h"

/* ------------------------------------------------------------------ */
/*  Tipos internos                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Nodo de la lista enlazada que almacena una tupla completa.
 *
 * Cada campo se corresponde directamente con un parámetro de la API:
 *  - key      → clave de búsqueda.
 *  - value1   → cadena de texto asociada.
 *  - N_value2 → dimensión real del vector V_value2.
 *  - V_value2 → vector de floats de longitud N_value2.
 *  - value3   → estructura Paquete con tres enteros.
 *  - next     → puntero al siguiente nodo (NULL en el último).
 */
typedef struct Elemento {
    char key[256];
    char value1[256];
    int  N_value2;
    float V_value2[32];
    struct Paquete value3;
    struct Elemento *next;
} Elemento;

/* ------------------------------------------------------------------ */
/*  Estado global del módulo                                            */
/* ------------------------------------------------------------------ */

/** Cabecera de la lista enlazada; NULL cuando la lista está vacía. */
static Elemento *lista = NULL;

/**
 * @brief Mutex que serializa el acceso a la lista enlazada.
 *
 * Se inicializa de forma estática con el valor por defecto de POSIX,
 * por lo que no se necesita llamar a pthread_mutex_init().
 */
static pthread_mutex_t mutex_lista = PTHREAD_MUTEX_INITIALIZER;


/* ------------------------------------------------------------------ */
/*  Implementación de la API                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Elimina todas las tuplas y deja la lista vacía.
 *
 * Recorre la lista liberando cada nodo con free().  Al finalizar,
 * el puntero de cabecera se resetea a NULL.
 *
 * @return Siempre 0 (la operación no puede fallar).
 */
int destroy(void) {
    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        Elemento *siguiente = actual->next;
        free(actual);
        actual = siguiente;
    }
    lista = NULL;   /* La lista queda formalmente vacía */

    pthread_mutex_unlock(&mutex_lista);
    return 0;
}


/**
 * @brief Inserta una nueva tupla al principio de la lista (O(1)).
 *
 * Antes de insertar se verifica que:
 *  1. Ningún puntero de entrada es NULL.
 *  2. N_value2 está en el rango [1, 32].
 *  3. La clave no existe ya en la lista (sin duplicados).
 *
 * @return 0 si se insertó con éxito, -1 en cualquier caso de error.
 */
int set_value(char *key, char *value1, int N_value2,
              float *V_value2, struct Paquete value3) {

    /* Validación de parámetros antes de adquirir el mutex */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32)                     return -1;
    if (strlen(key) > 255 || strlen(value1) > 255)          return -1;

    pthread_mutex_lock(&mutex_lista);

    /* Recorrido para detectar clave duplicada */
    Elemento *temp = lista;
    while (temp != NULL) {
        if (strcmp(temp->key, key) == 0) {
            pthread_mutex_unlock(&mutex_lista);
            return -1;  /* Clave duplicada: no se inserta */
        }
        temp = temp->next;
    }

    /* Reservar e inicializar el nuevo nodo */
    Elemento *nuevo = (Elemento *)malloc(sizeof(Elemento));
    if (nuevo == NULL) {
        /* Error de asignación de memoria */
        pthread_mutex_unlock(&mutex_lista);
        return -1;
    }

    strncpy(nuevo->key,    key,    255); nuevo->key[255]    = '\0';
    strncpy(nuevo->value1, value1, 255); nuevo->value1[255] = '\0';
    nuevo->N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        nuevo->V_value2[i] = V_value2[i];
    }
    nuevo->value3 = value3;

    /* Inserción al inicio: O(1) y no requiere recorrer la lista */
    nuevo->next = lista;
    lista = nuevo;

    pthread_mutex_unlock(&mutex_lista);
    return 0;
}


/**
 * @brief Busca la clave y copia sus valores en los buffers de salida.
 *
 * Los buffers value1 y V_value2 deben tener espacio para al menos
 * 256 bytes y 32 floats respectivamente (ver claves.h).
 *
 * @return 0 si se encontró la clave, -1 si no existe o hay error.
 */
int get_value(char *key, char *value1, int *N_value2,
              float *V_value2, struct Paquete *value3) {

    if (key == NULL || value1 == NULL || N_value2 == NULL ||
        V_value2 == NULL || value3 == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Clave encontrada: copiar todos los campos de salida */
            strcpy(value1, actual->value1);
            *N_value2 = actual->N_value2;
            for (int i = 0; i < actual->N_value2; i++) {
                V_value2[i] = actual->V_value2[i];
            }
            *value3 = actual->value3;

            pthread_mutex_unlock(&mutex_lista);
            return 0;
        }
        actual = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return -1;  /* Clave no encontrada */
}


/**
 * @brief Actualiza los valores de una tupla existente.
 *
 * La clave debe existir previamente; si no, devuelve -1 sin modificar
 * nada.  Las mismas restricciones sobre N_value2 que en set_value.
 *
 * @return 0 si se modificó con éxito, -1 en caso de error.
 */
int modify_value(char *key, char *value1, int N_value2,
                 float *V_value2, struct Paquete value3) {

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32)                     return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Clave encontrada: sobreescribir los valores */
            strncpy(actual->value1, value1, 255);
            actual->value1[255] = '\0';
            actual->N_value2 = N_value2;
            for (int i = 0; i < N_value2; i++) {
                actual->V_value2[i] = V_value2[i];
            }
            actual->value3 = value3;

            pthread_mutex_unlock(&mutex_lista);
            return 0;
        }
        actual = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return -1;  /* Clave no encontrada */
}


/**
 * @brief Elimina el nodo cuya clave coincide con key.
 *
 * Mantiene un puntero al nodo anterior para poder relanzar el enlace
 * sin necesidad de una segunda pasada.
 *
 * @return 0 si se eliminó, -1 si la clave no existe o key es NULL.
 */
int delete_key(char *key) {
    if (key == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual   = lista;
    Elemento *anterior = NULL;  /* Puntero de seguimiento */

    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Saltar el nodo actual en la cadena de punteros */
            if (anterior == NULL) {
                lista = actual->next;   /* Era el nodo cabecera */
            } else {
                anterior->next = actual->next;
            }
            free(actual);

            pthread_mutex_unlock(&mutex_lista);
            return 0;
        }
        anterior = actual;
        actual   = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return -1;  /* Clave no encontrada */
}


/**
 * @brief Comprueba la existencia de una clave sin modificar la lista.
 *
 * @return 1 si existe, 0 si no existe, -1 si key es NULL.
 */
int exist(char *key) {
    if (key == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            pthread_mutex_unlock(&mutex_lista);
            return 1;   /* Encontrado */
        }
        actual = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return 0;   /* No encontrado */
}
