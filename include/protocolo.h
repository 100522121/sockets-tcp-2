/**
 * @file protocolo.h
 * @brief Definición del protocolo de comunicación entre proxy-sock y servidor-sock.
 * El protocolo usa mensajes de texto plano de máximo MSG_MAX bytes delimitados por '|'.
 *
 * Formato de petición: OPERACION|param1|param2|...\n
 * Formato de respuesta:
 *   OK\n                                    -> retorno 0, sin datos extra
 *   OK|value1|N_value2|f0|...|fN-1|x|y|z\n  -> retorno 0 con datos (GET)
 *   OK|0\n o OK|1\n                         -> retorno 0/1 (EXIST)
 *   ERR\n                                   -> retorno -1
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define MSG_MAX 4096            // Tamaño máximo de un mensaje (petición o respuesta) en bytes
#define DELIM "|"               // Delimitador de campos dentro de un mensaje
#define DELIM_CHAR '|'          // Carácter delimitador para strtok y similares

/* Códigos de operación (como cadenas de texto) */
#define OP_DESTROY  "DESTROY"   // DESTROY
#define OP_SET      "SET"       // SET|key|value1|N_value2|f0|f1|...|fN-1|x|y|z
#define OP_GET      "GET"       // GET|key
#define OP_MODIFY   "MODIFY"    // MODIFY|key|value1|N_value2|f0|f1|...|fN-1|x|y|z
#define OP_DELETE   "DELETE"    // DELETE|key
#define OP_EXIST    "EXIST"     // EXIST|key

/* Códigos de respuesta */
#define RESP_OK     "OK"
#define RESP_ERR    "ERR"

#endif
