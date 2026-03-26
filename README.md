# Evaluable 1: Servicio de Tuplas con Colas de Mensajes

**Autores:** Ariana Cornejo Infante (100522121) y Nicolás Lemus Yeguas (100522110)

Implementación en C de un servicio de almacenamiento de tuplas `<key, value1, value2, value3>` con dos versiones:
una **no distribuida** mediante una biblioteca local, y una **distribuida** utilizando **colas de mensajes POSIX**.

---

## Estructura del proyecto

- **claves.c**  
Implementa el servicio de almacenamiento de tuplas utilizando una **lista enlazada** protegida con **mutex** para garantizar operaciones atómicas.

- **claves.h**  
Define la API del servicio de tuplas:
destroy, set_value, get_value, modify_value, delete_key, exist

- **proxy-mq.c**  
Implementa la biblioteca cliente distribuida. Las llamadas a la API se transforman en peticiones enviadas al servidor mediante colas de mensajes POSIX.

- **servidor-mq.c**  
Servidor que recibe peticiones desde los clientes mediante colas de mensajes y ejecuta las operaciones usando `libclaves.so`. Cada petición se atiende en un hilo independiente, permitiendo concurrencia.

- **app-cliente.c**  
Aplicación cliente que utiliza la API de tuplas para ejecutar una batería de pruebas del servicio.

- **Makefile**  
Permite compilar todo el proyecto y generar las bibliotecas y ejecutables necesarios.

---

## Compilación

Para compilar todo el proyecto: `make`

Esto generará los siguientes archivos:

    libclaves.so
    libproxyclaves.so
    servidor_mq
    app-cliente
    app-cliente-dist

Para limpiar los archivos compilados: `make clean`

---

## Ejecución

### Versión no distribuida

El cliente utiliza directamente la biblioteca `libclaves.so`.

    ./app-cliente

En este caso todas las operaciones se ejecutan localmente.

---

### Versión distribuida

Primero se debe iniciar el servidor:

    ./servidor_mq

En otra terminal se ejecuta el cliente distribuido:

    ./app-cliente-dist

En esta versión el cliente utiliza `libproxyclaves.so`, que envía las peticiones al servidor mediante colas de mensajes POSIX.

---

## Funcionamiento del sistema distribuido

Arquitectura general:

    Cliente
       |
    libproxyclaves.so
       |
    Colas POSIX
       |
    Servidor (servidor_mq)
       |
    libclaves.so
       |
    Estructura de datos (lista enlazada)

**Flujo de ejecución:**

1. El cliente invoca una función de la API (set_value, get_value, etc.).
2. El proxy construye una petición.
3. La petición se envía a la cola del servidor.
4. El servidor recibe la petición y crea un hilo para atenderla.
5. El servidor ejecuta la operación usando `libclaves.so`.
6. El resultado se envía de vuelta al cliente mediante una cola de respuesta.

---

## Consideraciones de diseño

- Las tuplas se almacenan en una lista enlazada dinámica.
- Se utiliza un mutex (`pthread_mutex`) para garantizar que las operaciones sean atómicas.
- El servidor es concurrente, creando un hilo por cada petición recibida.
- El proxy genera colas de respuesta temporales únicas utilizando el PID del proceso y un contador.
- Los errores del sistema de comunicación devuelven -2, mientras que los errores del servicio devuelven -1.

---

## Plan de pruebas

El cliente `app-cliente.c` implementa una batería de pruebas para validar el servicio.

Las pruebas realizadas incluyen:

- Inicialización del servicio (`destroy`)
- Inserción de tuplas (`set_value`)
- Comprobación de existencia (`exist`)
- Recuperación de valores (`get_value`)
- Modificación de tuplas (`modify_value`)
- Casos de error (clave duplicada y N_value2 fuera de rango)
- Eliminación de claves (`delete_key`)

Cada prueba muestra en pantalla si la operación se ejecuta correctamente o si se produce error.