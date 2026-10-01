# Arquitectura del sistema SMCD

## Componentes

| Componente | Lenguaje | Carpeta | Comunicación |
|---|---|---|---|
| Servidor (estación de control) | C, sockets Berkeley, pthreads | `servidor/` | TCP y UDP en el puerto `P` |
| Nodo orbital | Python | `nodo/` | TCP hacia `P` para registrarse, UDP hacia `P` para reportes y eventos |
| Cliente de operaciones | Python | `cliente/` | TCP hacia `P` |
| Módulo de protocolo compartido | Python | `comun/` | Lo usan el nodo y el cliente |
| Servicio de autenticación | Python + PostgreSQL | `auth/` | TCP, solo con el servidor |

El protocolo está especificado en [protocolo_SMCD.md](protocolo_SMCD.md).

## Módulos del servidor

| Módulo | Responsabilidad |
|---|---|
| `main.c` | Argumentos, creación de sockets, arranque de hilos, apagado |
| `red.c` | Sockets con `getaddrinfo`, lectura y escritura completas, dirección a texto |
| `protocolo.c` | Cabecera, parsing y construcción del payload, validación de campos |
| `log.c` | Registro en consola y archivo de cada mensaje recibido y enviado |
| `nodos.c` | Tabla de nodos, historial y eventos circulares, ventana anti-duplicados |
| `sesiones.c` | Tabla de sesiones autenticadas |
| `tcp_handler.c` | Atención de una conexión TCP: `REG_NODE`, `AUTH_REQUEST`, `QUERY` |
| `udp_handler.c` | Procesamiento de un datagrama: `STATE_REPORT`, `EVENT` |
| `monitor.c` | Detección de nodos inactivos |
| `auth_cliente.c` | Consulta al servicio de autenticación |

Las constantes del protocolo están en `include/smcd.h`.

## Hilos del servidor

| Hilo | Cantidad | Función |
|---|---|---|
| Principal | 1 | `accept()` en bucle; crea un hilo por conexión, hasta `MAX_CLIENTES` |
| Conexión TCP | 1 por conexión | Lee mensajes, responde y termina al cerrarse la conexión |
| UDP | 1 | `recvfrom()` en bucle sobre el socket UDP |
| Monitor | 1 | Cada segundo marca como `INACTIVO` los nodos sin contacto reciente |

En la Parte 2 el servidor es iterativo y usa `select()` sobre el socket TCP de escucha y el socket UDP. En la Parte 3 las mismas funciones de atención pasan a ejecutarse en los hilos de la tabla.

## Estado compartido

### Tabla de nodos

Arreglo fijo de `MAX_NODOS` entradas. Cada entrada guarda:

| Dato | Uso |
|---|---|
| `node_id`, `token`, `estado`, `intervalo` | Registro y validación |
| Último contacto (reloj monotónico) | Detección de inactividad |
| Último `seq_num` de reporte | Descarte de reportes atrasados y conteo de pérdidas |
| Mayor `seq_num` de evento + mapa de 32 bits | Ventana anti-duplicados |
| Historial circular de `HIST_SIZE` reportes | Consultas `NODE`, `LIST` y `HISTORY` |
| Buffer circular de `HIST_SIZE` eventos | Consulta `EVENTS` |
| Contadores: reportes, perdidos, eventos | Consulta `NODE` |

### Tabla de sesiones

Arreglo fijo de `MAX_CLIENTES` entradas con usuario, rol y dirección de origen. Lo usa la consulta `SESSIONS`.

### Sincronización

| Recurso | Protección |
|---|---|
| Tabla de nodos | `mutex` de la tabla |
| Tabla de sesiones | `mutex` de sesiones |
| Archivo de log y consola | `mutex` del log |

Reglas:

1. Si un hilo necesita dos recursos, los toma en este orden: nodos → sesiones → log.
2. No se envía ni se escribe en sockets con un `mutex` tomado. Se copia lo necesario, se libera el `mutex` y después se responde.

## Formato del log

Una línea por mensaje recibido (`RX`) o enviado (`TX`):

```
<fecha-hora ISO 8601> <RX|TX> <TCP|UDP> <IP>:<puerto> <TIPO> seq=<n> len=<n> <payload>
```

Las contraseñas se reemplazan por `***` antes de escribir la línea.

## Configuración

| Parámetro | Fuente |
|---|---|
| Puerto del servidor | Primer argumento de `./servidor` |
| Archivo de log | Segundo argumento de `./servidor` |
| Servicio de autenticación | Variables `SMCD_AUTH_HOST` y `SMCD_AUTH_PORT` del servidor |
| Servidor (para nodos y clientes) | Argumentos `--host` y `--puerto` |
