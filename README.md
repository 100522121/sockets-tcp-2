## <img src="https://upload.wikimedia.org/wikipedia/commons/thumb/e/e6/Anagrama_uc3m.jpg/250px-Anagrama_uc3m.jpg?utm_source=commons.wikimedia.org&utm_campaign=gallery&utm_content=thumbnail" alt="logo UC3M">

# Ejercicio Evaluable 2: Sockets TCP

Servicio de almacenamiento de tuplas key-value1-value2-value3 sobre sockets TCP.
<p align="center">
    <img src="https://github.com/user-attachments/assets/f19fe3a3-5484-4507-bdef-f818673a4eb8" alt="imagen del servicio" width="550">
</p>

## Características generales
- El almacenamiento de tuplas no tiene límite fijo y no requiere software adicional. 
- La comunicación se realiza entre un cliente (`proxy-sock.c`) y un servidor concurrente (`servidor-sock.c`) en C.
- La dirección IP y el puerto del servidor se configuran mediante variables de entorno. 
- El protocolo entre `proxy-sock.c` y `servidor-sock.c` es independiente del lenguaje, ya que no se envían estructuras de C por el socket.

## Compilación
Para generar los ficheros:
- `libclaves.so` — biblioteca interna del servidor
- `libproxyclaves.so` — biblioteca del lado del cliente
- `servidor` — ejecutable del servidor
- `cliente` — ejecutable del cliente
```shell
make
```

Para limpiar los ficheros generados:

```shell
make clean
```

## Despliegue

Se necesitan al menos dos terminales. Se asume IP `localhost` y puerto `4500`.

- **Terminal 1: Servidor**
```shell
./servidor 4500              # ./servidor <PUERTO>
```

- **Terminal 2: Cliente**
```shell
export IP_TUPLAS=localhost
export PORT_TUPLAS=4500
./cliente
```

O bien, en una sola línea:
```shell
env IP_TUPLAS=localhost PORT_TUPLAS=4500 ./cliente
```

## Variables de entorno

| Variable | Descripción |
| --- | --- |
| `IP_TUPLAS` | Dirección IP o nombre del servidor (decimal-punto o dominio-punto) |
| `PORT_TUPLAS` | Puerto del servidor |