# Diagramas de secuencia SMCD

## 1. Registro de un nodo

```mermaid
sequenceDiagram
    participant N as Nodo SAT-01
    participant S as Servidor
    N->>N: getaddrinfo(estacion-control.orbital.local)
    N->>S: TCP connect
    N->>S: REG_NODE seq=1 node_id=SAT-01, intervalo=5
    S->>S: crea nodo, genera token, estado ACTIVO
    S->>N: REG_ACK seq=1 status=OK, token=9f3a1c7e, timeout=15
    S--xN: cierra la conexión TCP
```

## 2. Reportes periódicos con un reporte perdido

```mermaid
sequenceDiagram
    participant N as Nodo SAT-01
    participant S as Servidor
    N->>S: STATE_REPORT seq=10
    S->>S: guarda en historial
    N-xS: STATE_REPORT seq=11 (perdido)
    N->>S: STATE_REPORT seq=12
    S->>S: hueco detectado, perdidos += 1
    N->>S: STATE_REPORT seq=12 (duplicado en la red)
    S->>S: no es posterior, se descarta
```

## 3. Evento con ACK perdido, retransmisión y duplicado

```mermaid
sequenceDiagram
    participant N as Nodo SAT-01
    participant S as Servidor
    N->>S: EVENT seq=7 event=OVERHEAT
    S->>S: seq nuevo, guarda el evento
    S-xN: EVENT_ACK seq=7 status=OK (perdido)
    Note over N: vence T_ACK (2 s), intento 1 de 3
    N->>S: EVENT seq=7 (retransmisión)
    S->>S: seq ya marcado en la ventana, no reprocesa
    S->>N: EVENT_ACK seq=7 status=DUP
    N->>N: evento confirmado
```

## 4. Evento fallido tras agotar los reintentos

```mermaid
sequenceDiagram
    participant N as Nodo SAT-01
    participant S as Servidor
    N-xS: EVENT seq=8 (perdido)
    Note over N: vence T_ACK, reintento 1
    N-xS: EVENT seq=8 (perdido)
    Note over N: vence T_ACK, reintento 2
    N-xS: EVENT seq=8 (perdido)
    Note over N: vence T_ACK, reintento 3
    N-xS: EVENT seq=8 (perdido)
    Note over N: vence T_ACK sin reintentos
    N->>N: evento FALLIDO, estado INACTIVO
    N->>S: TCP REG_NODE (re-registro)
    S->>N: REG_ACK con token nuevo
```

## 5. Autenticación y consulta de historial

```mermaid
sequenceDiagram
    participant C as Cliente
    participant S as Servidor
    participant A as Servicio auth
    participant D as PostgreSQL
    C->>S: TCP connect
    C->>S: AUTH_REQUEST seq=1 user=ana, pass=...
    S->>A: TCP connect + AUTH_REQUEST user=ana, pass=...
    A->>D: SELECT hash, sal, rol WHERE username = $1
    D->>A: fila
    A->>A: verifica el hash
    A->>S: AUTH_RESPONSE status=OK, role=admin
    S--xA: cierra la conexión
    S->>C: AUTH_RESPONSE seq=1 status=OK, role=admin
    C->>S: QUERY seq=2 q=HISTORY, node_id=SAT-01, n=5
    S->>C: QUERY_RESPONSE seq=2 count=5, h0_..., h4_...
```

## 6. Mensajes inválidos y errores

```mermaid
sequenceDiagram
    participant C as Cliente
    participant S as Servidor
    participant N as Nodo
    C->>S: QUERY seq=1 q=LIST (sin autenticar)
    S->>C: ERROR seq=1 code=6 NOT_AUTHENTICATED
    C->>S: AUTH_REQUEST seq=2 payload "user=ana" (sin punto y coma final)
    S->>C: ERROR seq=2 code=1 MALFORMED
    Note over C,S: la conexión TCP se mantiene
    N->>S: STATE_REPORT seq=43 cpu=150
    S->>N: ERROR seq=43 code=4 INVALID_PARAM (por UDP)
    C->>S: cabecera con version=2
    S->>C: ERROR code=2 BAD_VERSION
    S--xC: cierra la conexión
```

## 7. Nodo inactivo y re-registro

```mermaid
sequenceDiagram
    participant N as Nodo SAT-02
    participant S as Servidor
    N->>S: STATE_REPORT seq=20
    Note over N: el nodo deja de transmitir
    Note over S: 15 s sin mensajes (3 × intervalo)
    S->>S: SAT-02 INACTIVO, token invalidado
    Note over N: el nodo se recupera
    N->>S: STATE_REPORT seq=21 token viejo
    S->>N: ERROR seq=21 code=5 UNKNOWN_NODE
    N->>N: estado INACTIVO → REGISTRANDO
    N->>S: TCP REG_NODE node_id=SAT-02
    S->>N: REG_ACK token nuevo
    N->>S: STATE_REPORT seq=22 token nuevo
    S->>S: SAT-02 ACTIVO, historial conservado
```
