#ifndef CLAVES_H
#define CLAVES_H

/**
 * @file claves.h
 * @brief API pública del servicio de almacenamiento de tuplas.
 *
 * Define los tipos de datos compartidos y los prototipos de todas
 * las operaciones del servicio.  Este fichero es el único que deben
 * incluir tanto el cliente (app-cliente.c) como el servidor
 * (servidor-mq.c) y el proxy (proxy-mq.c).
 *
 * Convención de códigos de retorno:
 *   0   → éxito
 *  -1   → error lógico del servicio (clave inexistente, duplicada, rango…)
 *  -2   → error del sistema de comunicaciones (solo en versión distribuida)
 */

/* ------------------------------------------------------------------ */
/*  Tipos de datos                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Estructura que encapsula tres enteros para el tercer valor
 *        de una tupla.
 */
struct Paquete {
    int x;
    int y;
    int z;
};

/* ------------------------------------------------------------------ */
/*  Prototipos de la API                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Destruye todas las tuplas almacenadas e inicializa el servicio.
 *
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int destroy(void);

/**
 * @brief Inserta la tupla <key, value1, N_value2, V_value2, value3>.
 *
 * Se considera error:
 *  - Insertar una clave ya existente.
 *  - N_value2 fuera del rango [1, 32].
 *
 * @param key      Clave (hasta 255 caracteres).
 * @param value1   Cadena de hasta 255 caracteres.
 * @param N_value2 Dimensión del vector V_value2 (entre 1 y 32).
 * @param V_value2 Vector de floats de longitud N_value2.
 * @param value3   Estructura Paquete.
 * @return 0 si se insertó con éxito, -1 en caso de error.
 */
int set_value(char *key, char *value1, int N_value2,
              float *V_value2, struct Paquete value3);

/**
 * @brief Recupera los valores asociados a la clave key.
 *
 * Los buffers value1 y V_value2 deben tener capacidad para
 * 256 bytes y 32 floats respectivamente.
 *
 * @param key      Clave a buscar.
 * @param value1   Buffer de salida para la cadena (≥ 256 bytes).
 * @param N_value2 Salida: dimensión del vector recuperado.
 * @param V_value2 Buffer de salida para el vector (≥ 32 floats).
 * @param value3   Salida: estructura Paquete asociada.
 * @return 0 en caso de éxito, -1 si la clave no existe u otro error.
 */
int get_value(char *key, char *value1, int *N_value2,
              float *V_value2, struct Paquete *value3);

/**
 * @brief Modifica los valores asociados a la clave key.
 *
 * La clave debe existir previamente.  N_value2 debe estar en [1, 32].
 *
 * @param key      Clave del elemento a modificar.
 * @param value1   Nuevo valor de cadena.
 * @param N_value2 Nueva dimensión del vector (entre 1 y 32).
 * @param V_value2 Nuevo vector de floats.
 * @param value3   Nueva estructura Paquete.
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int modify_value(char *key, char *value1, int N_value2,
                 float *V_value2, struct Paquete value3);

/**
 * @brief Elimina la tupla cuya clave es key.
 *
 * @param key Clave del elemento a borrar.
 * @return 0 en caso de éxito, -1 si la clave no existe u otro error.
 */
int delete_key(char *key);

/**
 * @brief Comprueba si existe una tupla con clave key.
 *
 * @param key Clave a buscar.
 * @return 1 si existe, 0 si no existe, -1 en caso de error.
 */
int exist(char *key);

#endif /* CLAVES_H */
