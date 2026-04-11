/**
 * @file app-cliente.c
 * @brief Plan de pruebas del servicio de tuplas.
 *
 * Este fichero ejecuta distintas pruebas usando la API definida
 * en claves.h. Sirve tanto para la versión distribuida como la
 * no distribuida, ya que no depende directamente de colas POSIX.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claves.h"


/* ---- Macros auxiliares para mostrar resultados ---- */
#define ASSERT(cond, msg) \
    do { \
        if (cond) printf("  [OK]    " msg "\n"); \
        else      printf("  [ERROR] " msg "\n"); \
    } while (0)

#define ASSERT_RET(ret, esperado, msg) \
    do { \
        if ((ret) == (esperado)) \
            printf("  [OK]    " msg " (ret=%d)\n", (ret)); \
        else \
            printf("  [ERROR] " msg " (esperado=%d, obtenido=%d)\n", \
                   (esperado), (ret)); \
    } while (0)


/* ---- main ---- */
/**
 * @brief Plan de pruebas del servicio de tuplas.
 *
 * Este fichero ejecuta distintas pruebas usando la API definida
 * en claves.h. Sirve tanto para la versión distribuida como la
 * no distribuida, ya que no depende directamente de colas POSIX.
 *
 * Se comprueban:
 *  - Grupo 1: Operaciones básicas correctas
 *  - Grupo 2: Casos de error lógico (-1)
 *  - Grupo 3: Pruebas de comunicación (solo versión distribuida)
 *  - Grupo 4: Borrado de claves (delete_key)
 *  - Grupo 5: destroy() con datos existentes
 */
int main(void) {
    printf("--------------------------------------------------\n");
    printf(" Plan de pruebas - Servicio de Tuplas\n");
    printf("--------------------------------------------------\n\n");

    /* Datos comunes de prueba */
    char *key1 = "clave1";
    char *val1 = "valor_string_1";

    int n2 = 3;
    float v2[] = {1.1f, 2.2f, 3.3f};

    struct Paquete p = {10, 20, 30};

    char res_val1[256];
    int res_n2;
    float res_v2[32];
    struct Paquete res_p;

    int ret;


    /* ---------------------------------------------------------- */
    printf("Grupo 1 - Operaciones básicas\n");
    /* ---------------------------------------------------------- */
    /* 1.1 destroy */
    ret = destroy();
    ASSERT_RET(ret, 0, "1.1 destroy() limpia el estado inicial");

    /* 1.2 set_value */
    ret = set_value(key1, val1, n2, v2, p);
    ASSERT_RET(ret, 0, "1.2 set_value() inserta tupla valida");

    /* 1.3 exist con clave existente */
    ret = exist(key1);
    ASSERT_RET(ret, 1, "1.3 exist() devuelve 1 para clave existente");

    /* 1.4 exist con clave inexistente */
    ret = exist("no_existe");
    ASSERT_RET(ret, 0, "1.4 exist() devuelve 0 para clave inexistente");

    /* 1.5 get_value y verificar todos los campos */
    ret = get_value(key1, res_val1, &res_n2, res_v2, &res_p);
    ASSERT_RET(ret, 0, "1.5 get_value() correcto");
    ASSERT(strcmp(res_val1, val1) == 0, "value1 coincide");
    ASSERT(res_n2 == n2, "N_value2 coincide");
    ASSERT(res_v2[0] > 1.0f && res_v2[0] < 1.2f, "vector correcto");
    ASSERT(res_p.x == 10 && res_p.y == 20 && res_p.z == 30,
           "estructura correcta");

    /* 1.6 modify_value y verificar cambios */
    char *new_val1 = "valor_modificado";
    float new_v2[] = {9.9f, 2.2f, 3.3f};
    struct Paquete new_p = {100, 200, 300};

    ret = modify_value(key1, new_val1, n2, new_v2, new_p);
    ASSERT_RET(ret, 0, "1.6 modify_value() correcto");

    get_value(key1, res_val1, &res_n2, res_v2, &res_p);
    ASSERT(strcmp(res_val1, new_val1) == 0, "value1 actualizado");
    ASSERT(res_v2[0] > 9.8f, "vector actualizado");
    ASSERT(res_p.x == 100, "estructura actualizada");


    /* ---------------------------------------------------------- */
    printf("\nGrupo 2 - Casos de error (-1)\n");
    /* ---------------------------------------------------------- */
    /* 2.1 Clave duplicada */
    ret = set_value(key1, "duplicado", 1, v2, p);
    ASSERT_RET(ret, -1, "2.1 set_value() clave duplicada -> -1");

    /* 2.2 N_value2 > 32 */
    ret = set_value("clave2", "rango_alto", 33, v2, p);
    ASSERT_RET(ret, -1, "2.2 set_value() N_value2=33 -> -1");

    /* 2.3 N_value2 = 0 */
    ret = set_value("clave3", "rango_bajo", 0, v2, p);
    ASSERT_RET(ret, -1, "2.3 set_value() N_value2=0 -> -1");

    /* 2.4 value1 de 256 caracteres (> 255 permitidos) */
    char valor_largo[257];
    memset(valor_largo, 'A', 256);
    valor_largo[256] = '\0';
    ret = set_value("clave4", valor_largo, 1, v2, p);
    ASSERT_RET(ret, -1, "2.4 set_value() value1 de 256 chars -> -1");

    /* 2.5 get_value con clave inexistente */
    ret = get_value("no_existe", res_val1, &res_n2, res_v2, &res_p);
    ASSERT_RET(ret, -1, "2.5 get_value() clave inexistente -> -1");

    /* 2.6 modify_value con clave inexistente */
    ret = modify_value("no_existe", "nuevo", 1, v2, p);
    ASSERT_RET(ret, -1, "2.6 modify_value() clave inexistente -> -1");

    /* 2.7 delete_key con clave inexistente */
    ret = delete_key("no_existe");
    ASSERT_RET(ret, -1, "2.7 delete_key() clave inexistente -> -1");

    /* 2.8 Punteros NULL */
    ret = set_value(NULL, val1, 1, v2, p);
    ASSERT_RET(ret, -1, "2.8a set_value() key=NULL -> -1");

    ret = set_value(key1, NULL, 1, v2, p);
    ASSERT_RET(ret, -1, "2.8b set_value() value1=NULL -> -1");

    ret = get_value(NULL, res_val1, &res_n2, res_v2, &res_p);
    ASSERT_RET(ret, -1, "2.8c get_value() key=NULL -> -1");

    ret = modify_value(NULL, val1, 1, v2, p);
    ASSERT_RET(ret, -1, "2.8d modify_value() key=NULL -> -1");

    ret = delete_key(NULL);
    ASSERT_RET(ret, -1, "2.8e delete_key() key=NULL -> -1");

    ret = exist(NULL);
    ASSERT_RET(ret, -1, "2.8f exist() key=NULL -> -1");


    /* ---------------------------------------------------------- */
    printf("\nGrupo 3 - Comprobaciones IPC (solo distribuida)\n");
    /* ---------------------------------------------------------- */
    /*
     * Estas pruebas solo son observables en la versión DISTRIBUIDA, pues
     * la no distribuida resuelve las llamadas localmente y no devuelven -2.
     *
     * Cómo reproducir el error -2 manualmente:
     *   1. Compilar con: make app-cliente-mq
     *   2. NO iniciar el servidor (./servidor_mq).
     *   3. Ejecutar: ./app-cliente-mq
     *        Todas las llamadas deben devolver -2 (cola del servidor
     *        inexistente: mq_open falla con ENOENT).
     */
    printf("  [INFO] Prueba 3.1: Servidor no arrancado\n");
    printf("         En version distribuida sin servidor activo,\n");
    printf("         cualquier llamada debe devolver -2.\n");
    printf("         Ejecutar app-cliente-mq con el servidor detenido\n");
    printf("         para observar este comportamiento.\n");

    /*
     * La prueba 3.2 verifica que, si el servidor cae entre el envío
     * de la petición y la recepción de la respuesta, el proxy también
     * devuelve -2 (mq_receive falla o la cola de respuesta expira).
     * Este escenario requiere un test de integración manual.
     */
    printf("  [INFO] Prueba 3.2: Servidor cae durante la operacion\n");
    printf("         Requiere test de integracion manual:\n");
    printf("         1. Iniciar servidor.\n");
    printf("         2. Enviar peticion.\n");
    printf("         3. Matar servidor antes de que responda.\n");
    printf("         -> Proxy debe devolver -2.\n");


    /* ---------------------------------------------------------- */
    printf("\nGrupo 4 - delete_key\n");
    /* ---------------------------------------------------------- */
    /* 4.1 Borrar clave existente */
    ret = delete_key(key1);
    ASSERT_RET(ret, 0, "4.1 delete_key() clave existente -> 0");

    /* 4.2 Verificar que ya no existe */
    ret = exist(key1);
    ASSERT_RET(ret, 0, "4.2 exist() tras delete -> 0");


    /* ---------------------------------------------------------- */
    printf("\nGrupo 5 - destroy con varias claves\n");
    /* ---------------------------------------------------------- */
    /* 5.1 Insertar varias claves */
    set_value("a1", "val_a1", 1, v2, p);
    set_value("a2", "val_a2", 2, v2, p);
    set_value("a3", "val_a3", 3, v2, p);
    ASSERT(exist("a1") == 1 && exist("a2") == 1 && exist("a3") == 1,
           "5.1 Tres claves insertadas correctamente");

    /* 5.2 destroy y verificar que desaparecen */
    ret = destroy();
    ASSERT_RET(ret, 0, "5.2 destroy() con datos -> 0");
    ASSERT(exist("a1") == 0 && exist("a2") == 0 && exist("a3") == 0,
           "    Las tres claves han desaparecido tras destroy()");


    printf("\n--------------------------------------------------\n");
    printf(" Fin del plan de pruebas\n");
    printf("--------------------------------------------------\n");

    return 0;
}