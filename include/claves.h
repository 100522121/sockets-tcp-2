#ifndef CLAVES_H
#define CLAVES_H

struct Paquete {
    int x;
    int y;
    int z;
};

/**
 * @brief Inicializa el servicio destruyendo todas las tuplas almacenadas previamente.
 *
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int destroy(void);

/**
 * @brief Inserta el elemento <key, value1, N_value2, V_value2, value3>.
 *
 * Se considera error insertar una clave duplicada o que N_value2 esté fuera de [1,32].
 *
 * @param key      Clave (cadena de hasta 255 caracteres).
 * @param value1   Cadena de hasta 255 caracteres.
 * @param N_value2 Dimensión del vector V_value2 (entre 1 y 32).
 * @param V_value2 Vector de floats.
 * @param value3   Estructura Paquete.
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3);

/**
 * @brief Obtiene los valores asociados a la clave key.
 *
 * value1 y V_value2 deben tener espacio para 256 chars y 32 floats respectivamente.
 *
 * @param key      Clave a buscar.
 * @param value1   Buffer de salida para la cadena (mínimo 256 bytes).
 * @param N_value2 Salida: dimensión del vector.
 * @param V_value2 Buffer de salida para el vector (mínimo 32 floats).
 * @param value3   Salida: estructura Paquete.
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3);

/**
 * @brief Modifica los valores asociados a la clave key.
 *
 * Devuelve -1 si la clave no existe o N_value2 está fuera de [1,32].
 *
 * @param key      Clave del elemento a modificar.
 * @param value1   Nuevo valor de cadena.
 * @param N_value2 Nueva dimensión del vector (entre 1 y 32).
 * @param V_value2 Nuevo vector de floats.
 * @param value3   Nueva estructura Paquete.
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3);

/**
 * @brief Borra el elemento cuya clave es key.
 *
 * @param key Clave del elemento a borrar.
 * @return 0 en caso de éxito, -1 en caso de error (incluye clave inexistente).
 */
int delete_key(char *key);

/**
 * @brief Determina si existe un elemento con clave key.
 *
 * @param key Clave a buscar.
 * @return 1 si existe, 0 si no existe, -1 en caso de error.
 */
int exist(char *key);

#endif /* CLAVES_H */
