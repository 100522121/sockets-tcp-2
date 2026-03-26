#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claves.h"

/* --- Función auxiliar para mostrar el resultado de cada prueba --- */
static void print_test_result(const char *test_name, int result) {
    if (result == 0) {
        printf("[OK]    %s\n", test_name);
    } else if (result == 1) {
        printf("[OK]    %s (Resultado: 1 / Existe)\n", test_name);
    } else {
        printf("[ERROR] %s (Resultado: %d)\n", test_name, result);
    }
}

int main(void) {
    printf("=== Iniciando Plan de Pruebas ===\n\n");

    /* ---- Datos de prueba ---- */
    char *key1    = "clave1";
    char *val1    = "valor_string_1";
    int   n2      = 3;
    float v2[]    = {1.1f, 2.2f, 3.3f};
    struct Paquete p = {10, 20, 30};

    /* Buffers de salida reutilizables */
    char res_val1[256];
    int  res_n2;
    float res_v2[32];
    struct Paquete res_p;


    /* --- Prueba 1: destroy() — limpia el estado inicial --- */
    printf("--- Prueba 1: destroy() ---\n");
    print_test_result("destroy", destroy());

    /* --- Prueba 2: set_value() — insertar una tupla --- */
    printf("\n--- Prueba 2: set_value() ---\n");
    print_test_result("set_value clave1", set_value(key1, val1, n2, v2, p));

    /* --- Prueba 3: exist() — comprobar existencia --- */
    printf("\n--- Prueba 3: exist() ---\n");
    print_test_result("exist clave1 (debe ser 1)", exist(key1));

    if (exist("no_existe") == 0) {
        printf("[OK]    exist clave inexistente (Resultado: 0)\n");
    } else {
        printf("[ERROR] exist clave inexistente no devolvió 0\n");
    }

    /* --------------------------------------------------
     * Prueba 4: get_value() — obtener los valores
     * -------------------------------------------------- */
    printf("\n--- Prueba 4: get_value() ---\n");
    if (get_value(key1, res_val1, &res_n2, res_v2, &res_p) == 0) {
        printf("[OK]    get_value clave1\n");
        printf("        Val1   : %s\n", res_val1);
        printf("        N2     : %d\n", res_n2);
        printf("        V2     : %.1f  %.1f  %.1f\n", res_v2[0], res_v2[1], res_v2[2]);
        printf("        Paquete: x=%d  y=%d  z=%d\n", res_p.x, res_p.y, res_p.z);
    } else {
        printf("[ERROR] get_value clave1\n");
    }

    /* --- Prueba 5: modify_value() y verificación --- */
    printf("\n--- Prueba 5: modify_value() ---\n");
    char *new_val1 = "valor_modificado";
    v2[0] = 9.9f;
    p.x   = 100;
    print_test_result("modify_value clave1", modify_value(key1, new_val1, n2, v2, p));

    /* Verificar que los cambios se aplicaron correctamente */
    get_value(key1, res_val1, &res_n2, res_v2, &res_p);
    if (strcmp(res_val1, new_val1) == 0 && res_v2[0] > 9.8f && res_p.x == 100) {
        printf("[OK]    Verificación de modificación exitosa\n");
    } else {
        printf("[ERROR] Verificación de modificación fallida\n");
    }

    /* --- Prueba 6: casos de error controlados --- */
    printf("\n--- Prueba 6: Casos de error ---\n");

    /* Insertar clave duplicada debe devolver -1 */
    if (set_value(key1, "duplicado", 1, v2, p) == -1) {
        printf("[OK]    Error detectado al insertar clave duplicada\n");
    } else {
        printf("[ERROR] No se detectó error al insertar clave duplicada\n");
    }

    /* N_value2 fuera de rango (> 32) debe devolver -1 */
    if (set_value("clave2", "rango", 33, v2, p) == -1) {
        printf("[OK]    Error detectado con N_value2 fuera de rango (>32)\n");
    } else {
        printf("[ERROR] No se detectó error con N_value2 fuera de rango\n");
    }

    /* N_value2 = 0 (por debajo del rango) debe devolver -1 */
    if (set_value("clave3", "rango_bajo", 0, v2, p) == -1) {
        printf("[OK]    Error detectado con N_value2 fuera de rango (0)\n");
    } else {
        printf("[ERROR] No se detectó error con N_value2 = 0\n");
    }

    /* --- Prueba 7: delete_key() y comprobación posterior --- */
    printf("\n--- Prueba 7: delete_key() ---\n");
    print_test_result("delete_key clave1", delete_key(key1));

    if (exist(key1) == 0) {
        printf("[OK]    La clave fue borrada correctamente\n");
    } else {
        printf("[ERROR] La clave sigue existiendo tras delete_key\n");
    }

    /* Borrar una clave inexistente debe devolver -1 */
    if (delete_key("no_existe") == -1) {
        printf("[OK]    Error detectado al borrar clave inexistente\n");
    } else {
        printf("[ERROR] No se detectó error al borrar clave inexistente\n");
    }

    printf("\n=== Plan de Pruebas Finalizado ===\n");
    return 0;
}
