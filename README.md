# monitoreo_control_DC_orbitales

Sistema distribuido de monitoreo y control de centros de datos orbitales basado en el protocolo de aplicación **SMCD**.

- **Nodos orbitales**: centros de datos embarcados en satélites que reportan telemetría periódica y eventos críticos.
- **Estación de control terrestre** (servidor en C): registra nodos, guarda estado e historial y atiende consultas.
- **Clientes de operaciones**: se autentican y consultan el estado actual y el historial de cada nodo.

Nodos y clientes se comunican únicamente con el servidor.

## Estructura

| Carpeta | Contenido |
|---|---|
| `servidor/` | Estación de control en C (sockets Berkeley + pthreads) |

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
