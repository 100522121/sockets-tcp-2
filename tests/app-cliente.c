/**
 * @file app-cliente.c
 * @brief Plan de pruebas del servicio de tuplas.
 *
 * Este fichero es idéntico para la versión NO distribuida y la
 * DISTRIBUIDA: solo invoca funciones de la API definida en claves.h
 * y no contiene ninguna referencia a colas de mensajes POSIX.
 *
 * Plan de pruebas
 * ───────────────
 * Grupo 1 – Operaciones básicas con éxito
 *   1.1  destroy()          → limpiar el estado inicial
 *   1.2  set_value()        → insertar una tupla válida
 *   1.3  exist()            → comprobar existencia (debe existir)
 *   1.4  exist()            → comprobar existencia (no debe existir)
 *   1.5  get_value()        → recuperar y verificar todos los campos
 *   1.6  modify_value()     → modificar y verificar cambios
 *
 * Grupo 2 – Casos de error lógico (-1)
 *   2.1  set_value() con clave duplicada
 *   2.2  set_value() con N_value2 > 32
 *   2.3  set_value() con N_value2 = 0
 *   2.4  set_value() con value1 de 256 caracteres (fuera de rango)
 *   2.5  get_value() con clave inexistente
 *   2.6  modify_value() con clave inexistente
 *   2.7  delete_key() con clave inexistente
 *   2.8  Punteros NULL en set_value / get_value / modify_value
 *
 * Grupo 3 – Pruebas de comunicación (solo versión distribuida)
 *   [Nota: en la versión NO distribuida las funciones devuelven -1,
 *    no -2, ante parámetros NULL; el grupo 3 distingue -2 de -1.]
 *   3.1  Verificar que la llamada devuelve -2 si el servidor no está
 *        arrancado (solo observable en versión distribuida).
 *        → Aquí se documenta el comportamiento esperado.
 *
 * Grupo 4 – delete_key y comprobación posterior
 *   4.1  delete_key() con clave existente
 *   4.2  exist() sobre la clave recién borrada (debe devolver 0)
 *
 * Grupo 5 – destroy() con datos persistentes
 *   5.1  Insertar varias tuplas y luego llamar a destroy()
 *   5.2  Verificar que las claves han desaparecido
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claves.h"

/* ------------------------------------------------------------------ */
/*  Macros de ayuda para imprimir resultados                            */
/* ------------------------------------------------------------------ */

/** Imprime [OK] si @p cond es verdadera, [ERROR] en caso contrario. */
#define ASSERT(cond, msg) \
    do { \
        if (cond) printf("  [OK]    " msg "\n"); \
        else      printf("  [ERROR] " msg "\n"); \
    } while (0)

/** Imprime [OK] si @p ret == @p esperado. */
#define ASSERT_RET(ret, esperado, msg) \
    do { \
        if ((ret) == (esperado)) \
            printf("  [OK]    " msg " (ret=%d)\n", (ret)); \
        else \
            printf("  [ERROR] " msg " (esperado=%d, obtenido=%d)\n", \
                   (esperado), (ret)); \
    } while (0)

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     Plan de Pruebas — Servicio de Tuplas     ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* ── Datos de prueba comunes ─────────────────────────────────── */
    char *key1    = "clave1";
    char *val1    = "valor_string_1";
    int   n2      = 3;
    float v2[]    = {1.1f, 2.2f, 3.3f};
    struct Paquete p = {10, 20, 30};

    /* Buffers de salida reutilizables para get_value */
    char  res_val1[256];
    int   res_n2;
    float res_v2[32];
    struct Paquete res_p;
    int ret;


    /* ════════════════════════════════════════════════════════════ */
    printf("── Grupo 1: Operaciones básicas con éxito ──────────\n");
    /* ════════════════════════════════════════════════════════════ */

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
    ASSERT_RET(ret, 0, "1.5 get_value() devuelve 0 (exito)");
    ASSERT(strcmp(res_val1, val1) == 0,       "    value1 coincide");
    ASSERT(res_n2 == n2,                       "    N_value2 coincide");
    ASSERT(res_v2[0] > 1.0f && res_v2[0] < 1.2f, "    V_value2[0] ~ 1.1");
    ASSERT(res_p.x == 10 && res_p.y == 20 && res_p.z == 30,
           "    Paquete (x,y,z) coincide");

    /* 1.6 modify_value y verificar cambios */
    char *new_val1 = "valor_modificado";
    float new_v2[] = {9.9f, 2.2f, 3.3f};
    struct Paquete new_p = {100, 200, 300};
    ret = modify_value(key1, new_val1, n2, new_v2, new_p);
    ASSERT_RET(ret, 0, "1.6 modify_value() devuelve 0");
    get_value(key1, res_val1, &res_n2, res_v2, &res_p);
    ASSERT(strcmp(res_val1, new_val1) == 0, "    value1 actualizado");
    ASSERT(res_v2[0] > 9.8f,               "    V_value2[0] actualizado");
    ASSERT(res_p.x == 100,                  "    Paquete.x actualizado");


    /* ════════════════════════════════════════════════════════════ */
    printf("\n── Grupo 2: Casos de error lógico (-1) ─────────────\n");
    /* ════════════════════════════════════════════════════════════ */

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


    /* ════════════════════════════════════════════════════════════ */
    printf("\n── Grupo 3: Comportamiento ante errores IPC (MQ) ───\n");
    /* ════════════════════════════════════════════════════════════ */

    /*
     * NOTA: las pruebas de este grupo solo son observables en la
     * versión DISTRIBUIDA (app-cliente-mq enlazado con libproxyclaves.so).
     * En la versión no distribuida todas las llamadas resuelven
     * localmente y nunca devuelven -2.
     *
     * Cómo reproducir el error -2 manualmente:
     *   1. Compilar con: make app-cliente-mq
     *   2. NO iniciar el servidor (./servidor_mq).
     *   3. Ejecutar: ./app-cliente-mq
     *      → Todas las llamadas deben devolver -2 (cola del servidor
     *        inexistente: mq_open falla con ENOENT).
     *
     * La prueba 3.1 documenta este comportamiento esperado.
     * El valor -2 diferencia "servidor no arrancado" de "-1 lógico".
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


    /* ════════════════════════════════════════════════════════════ */
    printf("\n── Grupo 4: delete_key y comprobación posterior ────\n");
    /* ════════════════════════════════════════════════════════════ */

    /* 4.1 Borrar clave existente */
    ret = delete_key(key1);
    ASSERT_RET(ret, 0, "4.1 delete_key() clave existente -> 0");

    /* 4.2 Verificar que ya no existe */
    ret = exist(key1);
    ASSERT_RET(ret, 0, "4.2 exist() tras delete -> 0");


    /* ════════════════════════════════════════════════════════════ */
    printf("\n── Grupo 5: destroy() con datos persistentes ───────\n");
    /* ════════════════════════════════════════════════════════════ */

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


    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║          Plan de Pruebas Finalizado          ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    return 0;
}
