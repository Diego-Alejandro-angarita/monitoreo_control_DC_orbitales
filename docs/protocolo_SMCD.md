# SMCD — Protocolo de Monitoreo y Control de Centros de Datos Orbitales

**Versión del protocolo:** 1

## Índice

1. [Introducción](#1-introducción)
2. [Visión general](#2-visión-general)
3. [Primitivas de servicio](#3-primitivas-de-servicio)
4. [Formato de mensajes](#4-formato-de-mensajes)
5. [Definición de mensajes](#5-definición-de-mensajes)
6. [Reglas de procedimiento](#6-reglas-de-procedimiento)
7. [Temporizadores y constantes](#7-temporizadores-y-constantes)
8. [Máquinas de estado](#8-máquinas-de-estado)
9. [Códigos de error](#9-códigos-de-error)
10. [Ejemplos de intercambio](#10-ejemplos-de-intercambio)
11. [Consideraciones de seguridad](#11-consideraciones-de-seguridad)
12. [Justificación del transporte](#12-justificación-del-transporte)

---

## 1. Introducción

### 1.1 Propósito

SMCD es un protocolo de capa de aplicación para supervisar centros de datos embarcados en satélites. Define cómo los nodos orbitales se registran y reportan su estado, cómo notifican eventos críticos y cómo los operadores autenticados consultan el estado actual y el historial de cada nodo.

### 1.2 Alcance

Este documento especifica el formato de los mensajes, el transporte de cada uno, las reglas de procedimiento, los temporizadores, las máquinas de estado y el manejo de errores. No especifica la simulación de telemetría, la interfaz de usuario ni el almacenamiento interno de cada componente.

### 1.3 Terminología

Las palabras **DEBE**, **NO DEBE**, **DEBERÍA**, **NO DEBERÍA** y **PUEDE** se interpretan según el RFC 2119 (MUST, MUST NOT, SHOULD, SHOULD NOT, MAY).

### 1.4 Definiciones

| Término | Definición |
|---|---|
| Nodo | Centro de datos orbital. Se registra, reporta telemetría y notifica eventos. |
| Servidor | Estación de control terrestre. Único punto de contacto de nodos y clientes. |
| Cliente | Aplicación de operaciones usada por una persona autenticada. |
| Servicio de autenticación | Proceso separado que valida credenciales contra una base de datos. Solo el servidor se comunica con él. |
| Token | Identificador de 8 dígitos hexadecimales que el servidor entrega al nodo al registrarlo. Acompaña cada mensaje UDP del nodo. |
| Sesión | Conexión TCP de un cliente junto con su estado de autenticación y su rol. |
| Canal | Combinación de transporte (TCP o UDP) y conexión o par de direcciones por la que viaja un mensaje. |

---

## 2. Visión general

### 2.1 Arquitectura

```
 ┌────────┐  TCP: REG_NODE / REG_ACK            ┌──────────┐  TCP: AUTH_*, QUERY*   ┌─────────┐
 │  Nodo  │ ──────────────────────────────────► │          │ ◄────────────────────► │ Cliente │
 │        │  UDP: STATE_REPORT, EVENT/EVENT_ACK │ Servidor │                        └─────────┘
 └────────┘ ◄─────────────────────────────────► │          │
                                                └────┬─────┘
                                                     │ TCP: AUTH_REQUEST / AUTH_RESPONSE
                                              ┌──────▼───────┐
                                              │ Servicio de  │──► Base de datos
                                              │ autenticación│
                                              └──────────────┘
```

### 2.2 Puertos

- El servidor DEBE escuchar en un único número de puerto `P`, recibido por línea de comandos, tanto en TCP como en UDP.
- El servicio de autenticación escucha en un puerto TCP propio, configurado en el servidor.

### 2.3 Reglas generales

1. Los nodos y los clientes DEBEN comunicarse solo con el servidor. No existe comunicación nodo↔cliente.
2. El servidor NO DEBE iniciar comunicación con nodos ni clientes; solo responde a los mensajes que recibe.
3. Nodos y clientes DEBEN localizar al servidor por nombre de dominio (`getaddrinfo`). Ninguna dirección IP DEBE estar fija en el código. Si la resolución falla, el componente DEBE informar el error y reintentar sin terminar.
4. Solo el servidor envía mensajes `ERROR`. Un nodo o cliente que recibe un mensaje inválido DEBE descartarlo y registrarlo, sin responder.
5. Ningún componente DEBE responder a un mensaje `ERROR`.

---

## 3. Primitivas de servicio

| Primitiva | Tipo | Iniciador | Mensajes |
|---|---|---|---|
| `REGISTRAR.req` / `REGISTRAR.conf` | confirmada | Nodo | `REG_NODE` → `REG_ACK` |
| `REPORTAR_ESTADO.req` | no confirmada | Nodo | `STATE_REPORT` |
| `NOTIFICAR_EVENTO.req` / `NOTIFICAR_EVENTO.conf` | confirmada | Nodo | `EVENT` → `EVENT_ACK` |
| `AUTENTICAR.req` / `AUTENTICAR.conf` | confirmada | Cliente | `AUTH_REQUEST` → `AUTH_RESPONSE` |
| `CONSULTAR.req` / `CONSULTAR.conf` | confirmada | Cliente | `QUERY` → `QUERY_RESPONSE` |
| `ERROR.ind` | indicación | Servidor | `ERROR` |

---

## 4. Formato de mensajes

### 4.1 Cabecera

Todo mensaje comienza con una cabecera fija de 6 bytes. Los campos de 16 bits van en orden de red (big-endian).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    version    |     type      |            seq_num            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          payload_len          |  payload (payload_len bytes)  ~
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Campo | Tamaño | Descripción |
|---|---|---|
| `version` | 1 byte | Versión del protocolo. DEBE valer `1`. |
| `type` | 1 byte | Tipo de mensaje (§4.2). |
| `seq_num` | 2 bytes | Número de secuencia (§4.3). |
| `payload_len` | 2 bytes | Longitud del payload en bytes. Puede ser 0. |

Límites de `payload_len`:

| Transporte | Máximo | Razón |
|---|---|---|
| UDP | 1024 bytes | El datagrama (≤ 1030 bytes) cabe en un MTU Ethernet de 1500 bytes sin fragmentación IP. |
| TCP | 8192 bytes | Suficiente para la respuesta más grande (`QUERY` `LIST` con 64 nodos). |

### 4.2 Tipos de mensaje

| Código | Nombre | Transporte | Sentido |
|---|---|---|---|
| `0x01` | `REG_NODE` | TCP | Nodo → Servidor |
| `0x02` | `REG_ACK` | TCP | Servidor → Nodo |
| `0x03` | `STATE_REPORT` | UDP | Nodo → Servidor |
| `0x04` | `EVENT` | UDP | Nodo → Servidor |
| `0x05` | `EVENT_ACK` | UDP | Servidor → Nodo |
| `0x06` | `AUTH_REQUEST` | TCP | Cliente → Servidor, Servidor → Servicio de autenticación |
| `0x07` | `AUTH_RESPONSE` | TCP | Servidor → Cliente, Servicio de autenticación → Servidor |
| `0x08` | `QUERY` | TCP | Cliente → Servidor |
| `0x09` | `QUERY_RESPONSE` | TCP | Servidor → Cliente |
| `0x0F` | `ERROR` | El del mensaje que lo causó | Servidor → Nodo o Cliente |

### 4.3 Números de secuencia

1. Cada emisor mantiene contadores de 16 bits independientes: el nodo uno para `STATE_REPORT` y otro para `EVENT`; el cliente uno por conexión.
2. Cada mensaje nuevo incrementa el contador en 1, módulo 2¹⁶.
3. Una retransmisión DEBE conservar el `seq_num` del mensaje original.
4. Toda respuesta (`REG_ACK`, `EVENT_ACK`, `AUTH_RESPONSE`, `QUERY_RESPONSE`, `ERROR`) DEBE llevar el `seq_num` del mensaje que responde. Si la cabecera recibida no pudo leerse, el `ERROR` lleva `seq_num = 0`.
5. Para decidir si un número es posterior a otro se usa aritmética de números de serie (RFC 1982): `a` es posterior a `b` si `0 < (a − b) mod 2¹⁶ < 2¹⁵`.

### 4.4 Payload

El payload es texto ASCII con pares `clave=valor`, cada uno terminado en `;`.

```abnf
payload = *( par ";" )
par     = clave "=" valor
clave   = 1*32( ALPHA / DIGIT / "_" )
valor   = *128( %x20-3A / %x3C / %x3E-7E )   ; ASCII imprimible excepto ";" y "="
```

Reglas:

1. Una clave repetida hace que el payload sea inválido (`MALFORMED`).
2. Las claves desconocidas DEBEN ignorarse, para permitir extensiones.
3. El orden de los pares no tiene significado.
4. Ningún valor puede contener `;` ni `=`. Esto incluye las contraseñas.
5. Un mensaje recibido por el servidor PUEDE tener como máximo 32 pares; si tiene más, es `MALFORMED`.

### 4.5 Tipos de dato de los campos

| Tipo | Sintaxis | Ejemplo |
|---|---|---|
| entero | `["-"] 1*DIGIT` | `15` |
| decimal | `["-"] 1*DIGIT ["." 1*DIGIT]` | `38.5` |
| node_id | `1*16( ALPHA / DIGIT / "-" )` | `SAT-01` |
| token | `8HEXDIG` en minúsculas | `9f3a1c7e` |
| usuario | `1*32( ALPHA / DIGIT / "_" / "." / "-" )` | `ana` |
| texto | `valor` de la ABNF | `cpu fuera de rango` |

---

## 5. Definición de mensajes

En cada tabla, **O** = obligatorio y **Op** = opcional. Los ejemplos muestran la cabecera en hexadecimal seguida del payload en texto.

### 5.1 REG_NODE (`0x01`)

Nodo → Servidor, TCP. Solicita el registro del nodo.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `node_id` | O | node_id | Único por nodo |
| `intervalo` | O | entero | 1–60 segundos entre reportes |
| `tipo` | Op | texto | Tipo de órbita, por ejemplo `LEO`, `MEO`, `GEO` |
| `version_sw` | Op | texto | Versión del software del nodo |

```
01 01 00 01 00 33
node_id=SAT-01;intervalo=5;tipo=LEO;version_sw=1.0;
```

### 5.2 REG_ACK (`0x02`)

Servidor → Nodo, TCP. Confirma el registro.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `status` | O | texto | `OK` |
| `token` | O | token | Válido hasta el próximo registro o hasta que el nodo pase a inactivo |
| `intervalo` | O | entero | Intervalo aceptado |
| `timeout` | O | entero | Segundos sin mensajes tras los cuales el servidor marca el nodo inactivo |

```
01 02 00 01 00 30
status=OK;token=9f3a1c7e;intervalo=5;timeout=15;
```

### 5.3 STATE_REPORT (`0x03`)

Nodo → Servidor, UDP. Telemetría periódica, sin confirmación.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `node_id` | O | node_id | Nodo registrado |
| `token` | O | token | Token vigente |
| `ts` | O | entero | ≥ 0, segundos Unix según el reloj del nodo |
| `cpu` | O | decimal | 0–100 (% de uso) |
| `temp_int` | O | decimal | −50–150 (°C) |
| `power` | O | decimal | 0–100 (% de energía disponible) |
| `link` | O | texto | `UP`, `DEGRADED`, `DOWN` |

```
01 03 00 2a 00 56
node_id=SAT-01;token=9f3a1c7e;ts=1790000000;cpu=45.2;temp_int=38.5;power=87.0;link=UP;
```

### 5.4 EVENT (`0x04`)

Nodo → Servidor, UDP con confirmación (§6.4).

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `node_id` | O | node_id | Nodo registrado |
| `token` | O | token | Token vigente |
| `ts` | O | entero | ≥ 0 |
| `event` | O | texto | `OVERHEAT`, `LOW_POWER`, `LINK_LOST`, `HARDWARE_FAULT` |
| `severity` | O | texto | `WARNING`, `CRITICAL` |
| `detail` | Op | texto | Descripción libre |

```
01 04 00 07 00 62
node_id=SAT-01;token=9f3a1c7e;ts=1790000012;event=OVERHEAT;severity=CRITICAL;detail=temp_int 92.1;
```

### 5.5 EVENT_ACK (`0x05`)

Servidor → Nodo, UDP. Confirma un `EVENT` y lleva su mismo `seq_num`.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `status` | O | texto | `OK` si el evento es nuevo, `DUP` si ya se había recibido |

```
01 05 00 07 00 0a
status=OK;
```

### 5.6 AUTH_REQUEST (`0x06`)

Cliente → Servidor y Servidor → Servicio de autenticación, TCP.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `user` | O | usuario | — |
| `pass` | O | texto | 1–64 caracteres |

```
01 06 00 01 00 1c
user=ana;pass=Orbita-segura;
```

### 5.7 AUTH_RESPONSE (`0x07`)

Servidor → Cliente y Servicio de autenticación → Servidor, TCP.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `status` | O | texto | `OK`, `FAIL` |
| `role` | O si `OK` | texto | `admin`, `operador` |
| `reason` | Op | texto | Motivo del rechazo |

```
01 07 00 01 00 15
status=OK;role=admin;
```

### 5.8 QUERY (`0x08`)

Cliente → Servidor, TCP. Requiere sesión autenticada.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `q` | O | texto | `LIST`, `NODE`, `HISTORY`, `EVENTS`, `SESSIONS` |
| `node_id` | O para `NODE`, `HISTORY`, `EVENTS` | node_id | Nodo conocido por el servidor |
| `n` | Op | entero | 1–10, por defecto 5. Solo para `HISTORY` y `EVENTS` |

| `q` | Rol requerido | Devuelve |
|---|---|---|
| `LIST` | `operador` o `admin` | Estado actual de todos los nodos |
| `NODE` | `operador` o `admin` | Estado detallado de un nodo |
| `HISTORY` | `operador` o `admin` | Últimos `n` reportes de un nodo |
| `EVENTS` | `admin` | Últimos `n` eventos de un nodo |
| `SESSIONS` | `admin` | Sesiones de clientes autenticadas |

```
01 08 00 02 00 1d
q=HISTORY;node_id=SAT-01;n=2;
```

### 5.9 QUERY_RESPONSE (`0x09`)

Servidor → Cliente, TCP. Siempre incluye `status=OK`; los fallos se informan con `ERROR`. Las listas usan claves con índice: el índice `0` es el elemento más reciente o el primero de la lista.

**`LIST`**

| Clave | Descripción |
|---|---|
| `count` | Número de nodos |
| `n{i}_id`, `n{i}_estado` | Identificador y estado (`ACTIVO`, `INACTIVO`) |
| `n{i}_ts`, `n{i}_cpu`, `n{i}_temp`, `n{i}_power`, `n{i}_link` | Último reporte. Se omiten si el nodo aún no ha reportado |

**`NODE`**

| Clave | Descripción |
|---|---|
| `node_id`, `estado`, `intervalo` | Datos del registro |
| `ultimo_visto` | Segundos desde el último mensaje válido del nodo |
| `reportes`, `perdidos`, `eventos` | Contadores de reportes recibidos, reportes perdidos (huecos de `seq_num`) y eventos |
| `ts`, `cpu`, `temp`, `power`, `link` | Último reporte. Se omiten si no hay |

**`HISTORY`**

| Clave | Descripción |
|---|---|
| `node_id`, `count` | `count` = mínimo entre `n` y los reportes guardados |
| `h{i}_ts`, `h{i}_cpu`, `h{i}_temp`, `h{i}_power`, `h{i}_link` | Reporte `i` (0 = más reciente) |

**`EVENTS`**

| Clave | Descripción |
|---|---|
| `node_id`, `count` | — |
| `e{i}_ts`, `e{i}_event`, `e{i}_severity`, `e{i}_detail` | Evento `i` (0 = más reciente). `detail` se omite si no venía |

**`SESSIONS`**

| Clave | Descripción |
|---|---|
| `count` | — |
| `s{i}_user`, `s{i}_role`, `s{i}_origen` | Usuario, rol y `IP:puerto` del cliente |

```
01 09 00 02 00 a7
status=OK;node_id=SAT-01;count=2;h0_ts=1790000040;h0_cpu=47.0;h0_temp=39.1;h0_power=86.5;h0_link=UP;h1_ts=1790000035;h1_cpu=46.1;h1_temp=38.8;h1_power=86.8;h1_link=UP;
```

### 5.10 ERROR (`0x0F`)

Servidor → Nodo o Cliente, por el mismo canal del mensaje que lo causó.

| Clave | O/Op | Tipo | Rango o valores |
|---|---|---|---|
| `code` | O | entero | Código de §9 |
| `msg` | O | texto | Descripción legible |
| `ref_type` | Op | entero | `type` del mensaje que causó el error, si pudo leerse |

```
01 0f 00 2b 00 29
code=4;msg=cpu fuera de rango;ref_type=3;
```

---

## 6. Reglas de procedimiento

### 6.1 Validación de mensajes en el servidor

El servidor DEBE validar cada mensaje en este orden y responder con el primer error encontrado:

| Paso | Comprobación | Error |
|---|---|---|
| 1 | El datagrama UDP tiene al menos 6 bytes | `MALFORMED` |
| 2 | `version = 1` | `BAD_VERSION` |
| 3 | `payload_len` ≤ máximo del transporte | `TOO_LARGE` |
| 4 | En UDP, longitud del datagrama = 6 + `payload_len` | `MALFORMED` |
| 5 | `type` es uno de los de §4.2 | `UNKNOWN_TYPE` |
| 6 | El servidor acepta ese `type` en ese transporte (§6.1.1) | `WRONG_CHANNEL` |
| 7 | El payload cumple la ABNF y no repite claves | `MALFORMED` |
| 8 | Están los campos obligatorios y cumplen su tipo y rango | `INVALID_PARAM` |
| 9 | Reglas de estado: nodo registrado, sesión autenticada, rol | `UNKNOWN_NODE`, `NOT_AUTHENTICATED`, `FORBIDDEN` |

Un mensaje `ERROR` recibido por el servidor se registra y se descarta, sin respuesta.

#### 6.1.1 Mensajes que acepta el servidor

| Transporte | Tipos aceptados |
|---|---|
| TCP | `REG_NODE`, `AUTH_REQUEST`, `QUERY` |
| UDP | `STATE_REPORT`, `EVENT` |

#### 6.1.2 Lectura en TCP

TCP es un flujo de bytes. El receptor DEBE leer exactamente 6 bytes de cabecera y luego exactamente `payload_len` bytes, acumulando lecturas parciales. Tras un error de los pasos 5 a 9, el servidor ya leyó el mensaje completo, responde y mantiene la conexión. Tras `BAD_VERSION` o `TOO_LARGE` no puede confiar en la delimitación del flujo, por lo que responde y cierra la conexión.

### 6.2 Registro de nodos

1. El nodo resuelve el nombre del servidor, abre una conexión TCP y envía `REG_NODE`.
2. El servidor valida el mensaje:
   - Si el `node_id` no existe y hay espacio, crea el nodo.
   - Si el `node_id` no existe y la tabla está llena (`MAX_NODOS`), responde `ERROR SERVER_BUSY`.
   - Si el `node_id` ya existe, en cualquier estado, acepta el re-registro: conserva historial, eventos y contadores, y reinicia el seguimiento de `seq_num` y la ventana anti-duplicados, porque el nodo pudo reiniciar sus contadores.
3. El servidor genera un token nuevo, marca el nodo `ACTIVO`, actualiza su último contacto y responde `REG_ACK` con `timeout = FACTOR_INACTIVO × intervalo`.
4. El servidor cierra la conexión después de enviar `REG_ACK` o `ERROR`.
5. Si el nodo no recibe `REG_ACK` en `T_REG`, o falla la resolución o la conexión, DEBE reintentar con espera exponencial: 1, 2, 4, 8, 16 y luego 30 segundos.

### 6.3 Reporte de estado

1. Un nodo `ACTIVO` envía `STATE_REPORT` cada `intervalo` segundos.
2. Si el `node_id` no existe, el nodo está `INACTIVO` o el `token` no coincide, el servidor responde `ERROR UNKNOWN_NODE`.
3. Si es el primer reporte desde el registro, o su `seq_num` es posterior al último recibido:
   - suma a `perdidos` los números saltados (`seq_num − último − 1`);
   - guarda el reporte en el historial circular (`HIST_SIZE` entradas);
   - actualiza el último contacto.
4. Si el `seq_num` no es posterior al último, el reporte está duplicado o llegó tarde: el servidor lo descarta y lo registra en el log, sin responder.
5. Un reporte válido no recibe respuesta.

### 6.4 Notificación de eventos (UDP confiable)

**Nodo emisor:**

1. Envía `EVENT` con un `seq_num` nuevo del contador de eventos y arranca un temporizador `T_ACK`.
2. Si recibe `EVENT_ACK` con ese `seq_num`, el evento queda confirmado.
3. Si vence `T_ACK`, retransmite el mismo mensaje sin cambios, hasta `MAX_REINTENTOS` veces (4 envíos en total, unos 8 segundos como máximo).
4. Si se agotan los reintentos, el nodo DEBE registrar el evento como **fallido**, descartarlo y pasar a `INACTIVO`, porque supone que el enlace con el servidor está caído. Los eventos fallidos no se reenvían.
5. Puede haber varios eventos pendientes a la vez, cada uno con su propio temporizador.
6. Un `EVENT_ACK` con un `seq_num` que no está pendiente se descarta.

**Servidor:**

1. Valida `node_id` y `token` igual que en §6.3.
2. Mantiene por nodo una ventana anti-duplicados de `VENTANA_EVENTOS` números: el mayor `seq_num` recibido y un mapa de bits de los anteriores.

| Caso | Acción | Respuesta |
|---|---|---|
| `seq_num` posterior al mayor | Desplaza la ventana, marca, guarda el evento | `EVENT_ACK status=OK` |
| Dentro de la ventana y sin marcar | Marca y guarda el evento (llegó desordenado) | `EVENT_ACK status=OK` |
| Dentro de la ventana y ya marcado | No reprocesa | `EVENT_ACK status=DUP` |
| Anterior a la ventana | No reprocesa | `EVENT_ACK status=DUP` |

El reenvío del ACK ante un duplicado cubre el caso en que el ACK anterior se perdió. Dentro de un mismo registro, cada evento se procesa exactamente una vez.

3. Todo evento válido actualiza el último contacto del nodo y se guarda en un buffer circular de `HIST_SIZE` eventos.

### 6.5 Autenticación de clientes

1. El cliente resuelve el nombre del servidor y abre una conexión TCP. La sesión queda en estado `AUTENTICANDO`.
2. El cliente envía `AUTH_REQUEST`. El servidor la reenvía al servicio de autenticación (§6.8).

| Resultado | Respuesta al cliente | Estado de la sesión |
|---|---|---|
| Credenciales válidas | `AUTH_RESPONSE status=OK;role=…` | `AUTENTICADO`; se agrega a la tabla de sesiones |
| Credenciales inválidas | `AUTH_RESPONSE status=FAIL;reason=…` | Sigue en `AUTENTICANDO`; suma un fallo |
| Tercer fallo (`MAX_AUTH_FALLOS`) | `AUTH_RESPONSE status=FAIL` y cierre | `DESCONECTADO` |
| Servicio no disponible o sin respuesta en `T_SERVICIO_AUTH` | `ERROR AUTH_UNAVAILABLE` | Sigue en `AUTENTICANDO`; no suma fallo |

3. Un `AUTH_REQUEST` en una sesión ya autenticada recibe `ERROR INVALID_PARAM`.
4. El mensaje de rechazo NO DEBE indicar si el usuario existe.
5. Si una sesión en `AUTENTICANDO` pasa `T_AUTH` sin recibir mensajes, el servidor la cierra.

### 6.6 Consultas

1. Un `QUERY` en una sesión no autenticada recibe `ERROR NOT_AUTHENTICATED`.
2. Un `q` desconocido, o un `node_id` o `n` faltante o inválido, recibe `ERROR INVALID_PARAM`.
3. Un `node_id` que el servidor no conoce recibe `ERROR UNKNOWN_NODE`.
4. Un `q` que exige `admin` pedido por un `operador` recibe `ERROR FORBIDDEN`.
5. Si la sesión pasa `T_IDLE` sin recibir mensajes, el servidor la cierra.
6. El cliente DEBERÍA esperar cada respuesta como máximo `T_RESPUESTA`. Si no llega, o si la conexión se cae, DEBERÍA reconectar y volver a autenticarse.

### 6.7 Inactividad de nodos

1. El servidor revisa periódicamente el último contacto de cada nodo `ACTIVO`.
2. Si supera `FACTOR_INACTIVO × intervalo`, lo marca `INACTIVO`, invalida su token y lo registra en el log. El historial se conserva.
3. El siguiente mensaje UDP de ese nodo recibe `ERROR UNKNOWN_NODE`, y el nodo vuelve a registrarse (§6.2).

### 6.8 Interfaz con el servicio de autenticación

1. El servidor obtiene el nombre y el puerto del servicio de las variables de entorno `SMCD_AUTH_HOST` y `SMCD_AUTH_PORT`, y los resuelve con `getaddrinfo`.
2. Por cada `AUTH_REQUEST` de un cliente, el servidor abre una conexión TCP al servicio, envía un `AUTH_REQUEST` con los mismos `user` y `pass`, espera un `AUTH_RESPONSE` durante `T_SERVICIO_AUTH` y cierra.
3. El servicio responde `status=OK;role=…` o `status=FAIL;reason=…`. Guarda las contraseñas solo como hash con sal.
4. Cualquier falla (resolución, conexión, tiempo agotado o respuesta inválida) se informa al cliente como `ERROR AUTH_UNAVAILABLE`. El servidor sigue funcionando.

---

## 7. Temporizadores y constantes

| Nombre | Valor | Usado por | Significado |
|---|---|---|---|
| `T_REPORT` | 5 s | Nodo | Intervalo de reporte por defecto |
| `INTERVALO_MIN` / `INTERVALO_MAX` | 1 s / 60 s | Nodo, Servidor | Rango válido de `intervalo` |
| `FACTOR_INACTIVO` | 3 | Servidor | Inactivo tras `3 × intervalo` sin mensajes (15 s por defecto) |
| `T_ACK` | 2 s | Nodo | Espera de `EVENT_ACK` |
| `MAX_REINTENTOS` | 3 | Nodo | Retransmisiones de un `EVENT` |
| `T_REG` | 5 s | Nodo | Espera de `REG_ACK` |
| `T_REG_BACKOFF_MAX` | 30 s | Nodo | Espera máxima entre intentos de registro |
| `T_AUTH` | 30 s | Servidor | Inactividad máxima de una conexión no autenticada |
| `T_IDLE` | 300 s | Servidor | Inactividad máxima de una sesión autenticada |
| `T_SERVICIO_AUTH` | 3 s | Servidor | Espera de respuesta del servicio de autenticación |
| `T_RESPUESTA` | 5 s | Cliente | Espera de respuesta del servidor |
| `HIST_SIZE` | 10 | Servidor | Reportes y eventos guardados por nodo |
| `N_DEFECTO` | 5 | Servidor | Valor de `n` cuando no se envía |
| `N_MAX` | 10 | Servidor | Valor máximo de `n` |
| `VENTANA_EVENTOS` | 32 | Servidor | Tamaño de la ventana anti-duplicados |
| `MAX_NODOS` | 64 | Servidor | Nodos registrables |
| `MAX_CLIENTES` | 32 | Servidor | Conexiones TCP simultáneas |
| `MAX_AUTH_FALLOS` | 3 | Servidor | Intentos de autenticación por conexión |

---

## 8. Máquinas de estado

Los diagramas están en [diagramas/estados.md](diagramas/estados.md).

### 8.1 Nodo

| Estado | Evento | Acción | Estado siguiente |
|---|---|---|---|
| `DESCONECTADO` | Inicio del proceso | Resolver nombre, conectar, enviar `REG_NODE` | `REGISTRANDO` |
| `REGISTRANDO` | `REG_ACK` | Guardar token, iniciar reportes | `ACTIVO` |
| `REGISTRANDO` | `ERROR`, vence `T_REG`, falla DNS o conexión | Esperar backoff y reintentar | `REGISTRANDO` |
| `ACTIVO` | Vence el intervalo | Enviar `STATE_REPORT` | `ACTIVO` |
| `ACTIVO` | Se detecta una condición de evento | Enviar `EVENT`, iniciar `T_ACK` | `ACTIVO` |
| `ACTIVO` | `EVENT_ACK` | Quitar el evento de pendientes | `ACTIVO` |
| `ACTIVO` | Vence `T_ACK` con reintentos disponibles | Retransmitir con el mismo `seq_num` | `ACTIVO` |
| `ACTIVO` | Vence `T_ACK` sin reintentos | Registrar evento fallido | `INACTIVO` |
| `ACTIVO` | `ERROR UNKNOWN_NODE` | Descartar token | `INACTIVO` |
| `INACTIVO` | Inmediato | Detener reportes, descartar eventos pendientes, iniciar registro | `REGISTRANDO` |

### 8.2 Sesión de cliente

| Estado | Evento | Acción | Estado siguiente |
|---|---|---|---|
| `DESCONECTADO` | El cliente conecta | — | `AUTENTICANDO` |
| `AUTENTICANDO` | `AUTH_RESPONSE OK` | Guardar rol | `AUTENTICADO` |
| `AUTENTICANDO` | `AUTH_RESPONSE FAIL`, fallos < 3 | — | `AUTENTICANDO` |
| `AUTENTICANDO` | `AUTH_RESPONSE FAIL`, fallos = 3 | El servidor cierra | `DESCONECTADO` |
| `AUTENTICANDO` | `ERROR AUTH_UNAVAILABLE` | — | `AUTENTICANDO` |
| `AUTENTICANDO` | Vence `T_AUTH` | El servidor cierra | `DESCONECTADO` |
| `AUTENTICADO` | `QUERY` | `QUERY_RESPONSE` o `ERROR` | `AUTENTICADO` |
| `AUTENTICADO` | Vence `T_IDLE` | El servidor cierra | `DESCONECTADO` |
| Cualquiera | Cierre o caída de la conexión | Liberar la sesión | `DESCONECTADO` |

### 8.3 Nodo visto por el servidor

| Estado | Evento | Acción | Estado siguiente |
|---|---|---|---|
| `DESCONOCIDO` | `REG_NODE` válido | Crear nodo, generar token | `ACTIVO` |
| `ACTIVO` | `STATE_REPORT` o `EVENT` válido | Actualizar último contacto | `ACTIVO` |
| `ACTIVO` | `REG_NODE` válido | Token nuevo, reiniciar secuencias | `ACTIVO` |
| `ACTIVO` | Sin mensajes durante `FACTOR_INACTIVO × intervalo` | Invalidar token | `INACTIVO` |
| `INACTIVO` | `STATE_REPORT` o `EVENT` | `ERROR UNKNOWN_NODE` | `INACTIVO` |
| `INACTIVO` | `REG_NODE` válido | Token nuevo, reiniciar secuencias | `ACTIVO` |

### 8.4 Evento en el nodo

| Estado | Evento | Acción | Estado siguiente |
|---|---|---|---|
| `PENDIENTE` | Envío inicial | Arrancar `T_ACK`, intentos = 0 | `ESPERANDO_ACK` |
| `ESPERANDO_ACK` | `EVENT_ACK` | — | `CONFIRMADO` |
| `ESPERANDO_ACK` | Vence `T_ACK`, intentos < 3 | Retransmitir, intentos + 1 | `ESPERANDO_ACK` |
| `ESPERANDO_ACK` | Vence `T_ACK`, intentos = 3 | Registrar evento fallido | `FALLIDO` |

---

## 9. Códigos de error

| Código | Nombre | Significado | Transporte | Conexión TCP tras el error |
|---|---|---|---|---|
| 1 | `MALFORMED` | Cabecera incompleta, longitud inconsistente o payload que no cumple la ABNF | TCP, UDP | Se mantiene |
| 2 | `BAD_VERSION` | `version` distinta de 1 | TCP, UDP | Se cierra |
| 3 | `UNKNOWN_TYPE` | `type` no definido | TCP, UDP | Se mantiene |
| 4 | `INVALID_PARAM` | Campo obligatorio faltante, fuera de rango o petición no válida en el estado actual | TCP, UDP | Se mantiene |
| 5 | `UNKNOWN_NODE` | Nodo no registrado, inactivo o con token incorrecto | TCP, UDP | Se mantiene |
| 6 | `NOT_AUTHENTICATED` | `QUERY` sin sesión autenticada | TCP | Se mantiene |
| 7 | `FORBIDDEN` | El rol no permite la consulta | TCP | Se mantiene |
| 8 | `TOO_LARGE` | `payload_len` supera el máximo | TCP, UDP | Se cierra |
| 9 | `WRONG_CHANNEL` | Tipo válido que el servidor no acepta en ese transporte | TCP, UDP | Se mantiene |
| 10 | `AUTH_UNAVAILABLE` | El servicio de autenticación no respondió | TCP | Se mantiene |
| 11 | `SERVER_BUSY` | Se alcanzó `MAX_CLIENTES` o `MAX_NODOS` | TCP | Se cierra |
| 12 | `INTERNAL` | Falla interna del servidor | TCP, UDP | Se mantiene |

---

## 10. Ejemplos de intercambio

Los diagramas de secuencia están en [diagramas/secuencias.md](diagramas/secuencias.md):

1. Registro de un nodo
2. Reportes periódicos con un reporte perdido
3. Evento con ACK perdido, retransmisión y duplicado
4. Evento fallido tras agotar los reintentos
5. Autenticación y consulta de historial
6. Mensajes inválidos y errores
7. Nodo inactivo y re-registro

Traza de una consulta sin autenticación:

```
Cliente → Servidor   01 08 00 01 00 07   q=LIST;
Servidor → Cliente   01 0f 00 01 00 2c   code=6;msg=sesion no autenticada;ref_type=8;
```

---

## 11. Consideraciones de seguridad

1. **Credenciales en claro.** SMCD no cifra. Usuario y contraseña viajan sin cifrar del cliente al servidor y del servidor al servicio de autenticación. Un despliegue real DEBERÍA usar TLS.
2. **Almacenamiento de contraseñas.** El servicio de autenticación guarda solo un hash con sal (scrypt). Ni el servidor ni los clientes guardan contraseñas.
3. **Suplantación de nodos.** El token impide que un tercero que solo conoce el `node_id` envíe reportes o eventos válidos, pero cualquiera que capture el tráfico puede leerlo. Es una protección básica, no autenticación fuerte.
4. **Fuerza bruta.** `MAX_AUTH_FALLOS` limita los intentos por conexión.
5. **Agotamiento de recursos.** Los límites de `payload_len`, `MAX_CLIENTES`, `MAX_NODOS`, `T_AUTH` y `T_IDLE` acotan la memoria y las conexiones que un atacante puede ocupar.
6. **Enumeración de usuarios.** El rechazo no revela si el usuario existe.
7. **Inyección SQL.** El servicio de autenticación DEBE usar consultas parametrizadas.

---

## 12. Justificación del transporte

| Mensajes | Transporte | Justificación |
|---|---|---|
| `REG_NODE` / `REG_ACK` | TCP | El registro entrega el token del que dependen todos los mensajes posteriores; no puede perderse ni duplicarse. Ocurre pocas veces, así que el costo de abrir una conexión es despreciable. |
| `AUTH_*`, `QUERY` / `QUERY_RESPONSE` | TCP | Son intercambios petición-respuesta que no toleran pérdida ni desorden. La sesión autenticada queda ligada a la conexión. Las respuestas pueden ocupar varios KB. |
| `STATE_REPORT` | UDP | Son periódicos e idempotentes: si uno se pierde, el siguiente lo reemplaza en pocos segundos. UDP evita mantener una conexión por nodo y el bloqueo de cabeza de línea de TCP, donde un segmento perdido retrasa a los siguientes. |
| `EVENT` / `EVENT_ACK` | UDP con confirmación propia | Los eventos son críticos, pero pequeños y esporádicos. Mantener una conexión TCP abierta por nodo solo para ellos cuesta recursos. Con ACK, `T_ACK`, reintentos y deduplicación se obtiene la confiabilidad necesaria, con un tiempo máximo de recuperación conocido (unos 8 s), y la falla se detecta explícitamente. |
| `ERROR` | El del mensaje que lo causó | El emisor recibe el error por el mismo camino por el que espera la respuesta. |
