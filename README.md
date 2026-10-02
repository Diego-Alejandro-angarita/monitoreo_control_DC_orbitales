# monitoreo_control_DC_orbitales

Sistema distribuido de monitoreo y control de centros de datos orbitales basado en el protocolo de aplicación **SMCD**.

- **Nodos orbitales**: centros de datos embarcados en satélites que reportan telemetría periódica y eventos críticos.
- **Estación de control terrestre** (servidor en C): registra nodos, guarda estado e historial y atiende consultas.
- **Clientes de operaciones**: se autentican y consultan el estado actual y el historial de cada nodo.

Nodos y clientes se comunican únicamente con el servidor.

El protocolo está especificado en [docs/protocolo_SMCD.md](docs/protocolo_SMCD.md).

## Estructura

| Carpeta | Contenido |
|---|---|
| `servidor/` | Estación de control en C (sockets Berkeley + pthreads) |
| `comun/` | Módulo Python del protocolo, compartido por nodos y clientes |
| `nodo/` | Simulador de nodo orbital |
| `cliente/` | Cliente de operaciones de consola |
| `pruebas/` | Pruebas automáticas |
| `docs/` | Especificación del protocolo, arquitectura y diagramas |

## Requisitos

- Linux o WSL2
- `gcc` y `make`
- Python 3.10 o superior (nodos y clientes)

## Compilación del servidor

```bash
make -C servidor
```

Otros objetivos:

| Objetivo | Uso |
|---|---|
| `make -C servidor test` | Ejecuta las pruebas unitarias del protocolo |
| `make -C servidor debug` | Compila con símbolos y AddressSanitizer/UBSan |
| `make -C servidor tsan` | Compila con ThreadSanitizer |
| `make -C servidor clean` | Elimina binarios y objetos |

## Ejecución del servidor

```bash
mkdir -p logs
./servidor/servidor <puerto> <archivoDeLogs>
```

Ejemplo:

```bash
./servidor/servidor 9000 logs/servidor.log
```

El mismo número de puerto se usa para TCP y UDP.

Después de compilar con `debug` o `tsan`, ejecutar `make -C servidor clean` antes de volver a `make` o `make test`.

## Ejecución de un nodo

```bash
python3 nodo/nodo.py --host <nombre> --puerto <puerto> --id <node_id> [--intervalo <s>] [--tipo <orbita>]
```

Ejemplo:

```bash
python3 nodo/nodo.py --host localhost --puerto 9000 --id SAT-01
```

`--intervalo` va de 1 a 60 segundos (por defecto 5). Si el nombre no se resuelve o el servidor no responde, el nodo reintenta con espera creciente.

## Ejecución del cliente

```bash
python3 cliente/cliente.py --host <nombre> --puerto <puerto>
```

El menú permite ver el estado de todos los nodos, el detalle de uno, su historial, sus eventos y un monitor en vivo.

## Pruebas

```bash
make -C servidor test
python3 -m unittest pruebas/test_smcd.py
```

## Resolución de nombres

Los nodos y clientes no usan direcciones IP en el código: se conectan por nombre de dominio. Para pruebas locales, agregar a `/etc/hosts`:

```
127.0.0.1   estacion-control.orbital.local
127.0.0.1   auth.orbital.local
```

En WSL2, `/etc/hosts` se regenera al reiniciar. Para conservar los cambios, agregar en `/etc/wsl.conf`:

```
[network]
generateHosts = false
```
