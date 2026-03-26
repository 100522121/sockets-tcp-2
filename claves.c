#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "claves.h"

/* Estructura interna: nodo de la lista enlazada de tuplas */
typedef struct Elemento {
    char key[256];
    char value1[256];
    int N_value2;
    float V_value2[32];
    struct Paquete value3;
    struct Elemento *next;
} Elemento;

/* Puntero al inicio de la lista enlazada */
static Elemento *lista = NULL;

/* Mutex para garantizar la atomicidad de todas las operaciones */
static pthread_mutex_t mutex_lista = PTHREAD_MUTEX_INITIALIZER;


/* --- destroy: elimina todas las tuplas almacenadas --- */
int destroy(void) {
    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        Elemento *siguiente = actual->next;
        free(actual);
        actual = siguiente;
    }
    lista = NULL;

    pthread_mutex_unlock(&mutex_lista);
    return 0;
}


/* --- set_value: inserta una nueva tupla en la lista --- */
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    /* Validación de parámetros */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32) return -1;

    pthread_mutex_lock(&mutex_lista);

    /* Comprobar si la clave ya existe (no se permiten duplicados) */
    Elemento *temp = lista;
    while (temp != NULL) {
        if (strcmp(temp->key, key) == 0) {
            pthread_mutex_unlock(&mutex_lista);
            return -1;
        }
        temp = temp->next;
    }

    /* Reservar memoria para el nuevo nodo */
    Elemento *nuevo = (Elemento *)malloc(sizeof(Elemento));
    if (nuevo == NULL) {
        pthread_mutex_unlock(&mutex_lista);
        return -1;
    }

    /* Copiar los datos en el nuevo nodo */
    strncpy(nuevo->key, key, 255);
    nuevo->key[255] = '\0';
    strncpy(nuevo->value1, value1, 255);
    nuevo->value1[255] = '\0';
    nuevo->N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        nuevo->V_value2[i] = V_value2[i];
    }
    nuevo->value3 = value3;

    /* Insertar al inicio de la lista (O(1)) */
    nuevo->next = lista;
    lista = nuevo;

    pthread_mutex_unlock(&mutex_lista);
    return 0;
}


/* --- get_value: obtiene los valores asociados a una clave --- */
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    /* Validación de parámetros */
    if (key == NULL || value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Clave encontrada: copiar todos los valores de salida */
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
    return -1; /* Clave no encontrada */
}


/** --- modify_value: modifica los valores de una clave existente --- */
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    /* Validación de parámetros */
    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > 32) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Clave encontrada: actualizar los valores */
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
    return -1; /* Clave no encontrada */
}


/* --- delete_key: borra el elemento con clave key --- */
int delete_key(char *key) {
    if (key == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    Elemento *anterior = NULL;

    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            /* Enlazar el nodo anterior con el siguiente, saltando actual */
            if (anterior == NULL) {
                lista = actual->next; /* Era el primer nodo */
            } else {
                anterior->next = actual->next;
            }
            free(actual);

            pthread_mutex_unlock(&mutex_lista);
            return 0;
        }
        anterior = actual;
        actual = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return -1; /* Clave no encontrada */
}


/* --- exist: comprueba si una clave existe en la lista --- */
int exist(char *key) {
    if (key == NULL) return -1;

    pthread_mutex_lock(&mutex_lista);

    Elemento *actual = lista;
    while (actual != NULL) {
        if (strcmp(actual->key, key) == 0) {
            pthread_mutex_unlock(&mutex_lista);
            return 1; /* Encontrado */
        }
        actual = actual->next;
    }

    pthread_mutex_unlock(&mutex_lista);
    return 0; /* No encontrado */
}
