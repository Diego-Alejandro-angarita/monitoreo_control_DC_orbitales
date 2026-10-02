#!/usr/bin/env python3
"""Cliente de operaciones: consulta el estado y el historial de los nodos orbitales."""

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "comun"))
import smcd  # noqa: E402

INTERVALO_MONITOR = 2


class ErrorServidor(Exception):
    def __init__(self, campos):
        super().__init__(smcd.describir_error(campos))


class Sesion:
    def __init__(self, host, puerto):
        self.host = host
        self.puerto = puerto
        self.sock = None
        self.seq = 0

    def cerrar(self):
        if self.sock is not None:
            self.sock.close()
        self.sock = None

    def consultar(self, campos):
        for intento in (1, 2):
            try:
                if self.sock is None:
                    self.sock = smcd.conectar_tcp(self.host, self.puerto, smcd.T_RESPUESTA)
                return self.pedir(campos)
            except (OSError, smcd.ErrorProtocolo) as e:
                self.cerrar()
                if intento == 2:
                    raise
                print(f"Conexion perdida ({e}). Reconectando...")

    def pedir(self, campos):
        self.seq = smcd.siguiente_seq(self.seq)
        self.sock.sendall(smcd.armar(smcd.QUERY, self.seq, campos))
        tipo, seq, respuesta = smcd.recibir_tcp(self.sock)
        if seq != self.seq:
            raise smcd.ErrorProtocolo(f"seq de respuesta inesperado: {seq}")
        if tipo == smcd.ERROR:
            raise ErrorServidor(respuesta)
        if tipo != smcd.QUERY_RESPONSE:
            raise smcd.ErrorProtocolo(f"respuesta inesperada: {smcd.NOMBRES.get(tipo, tipo)}")
        return respuesta


def hora(ts):
    return time.strftime("%H:%M:%S", time.localtime(int(ts))) if ts else "-"


def tabla(encabezados, filas):
    anchos = [max(len(str(x)) for x in columna) for columna in zip(encabezados, *filas)]
    linea = "  ".join(f"{{:<{a}}}" for a in anchos)
    print(linea.format(*encabezados))
    print("  ".join("-" * a for a in anchos))
    for fila in filas:
        print(linea.format(*fila))
    if not filas:
        print("(sin datos)")


def filas_reportes(respuesta, prefijo, total):
    filas = []
    for i in range(total):
        p = f"{prefijo}{i}_"
        filas.append([
            hora(respuesta.get(p + "ts")),
            respuesta.get(p + "cpu", "-"),
            respuesta.get(p + "temp", "-"),
            respuesta.get(p + "power", "-"),
            respuesta.get(p + "link", "-"),
        ])
    return filas


def mostrar_lista(respuesta):
    total = int(respuesta.get("count", 0))
    filas = []
    for i, reporte in enumerate(filas_reportes(respuesta, "n", total)):
        filas.append([respuesta.get(f"n{i}_id"), respuesta.get(f"n{i}_estado")] + reporte)
    tabla(["NODO", "ESTADO", "HORA", "CPU %", "TEMP C", "POWER %", "LINK"], filas)


def mostrar_nodo(respuesta):
    etiquetas = [
        ("Nodo", "node_id"), ("Estado", "estado"), ("Intervalo (s)", "intervalo"),
        ("Ultimo contacto (s)", "ultimo_visto"), ("Reportes recibidos", "reportes"),
        ("Reportes perdidos", "perdidos"), ("Eventos", "eventos"), ("CPU %", "cpu"),
        ("Temperatura C", "temp"), ("Energia %", "power"), ("Enlace", "link"),
    ]
    for etiqueta, clave in etiquetas:
        print(f"{etiqueta:<22}{respuesta.get(clave, '-')}")
    print(f"{'Ultimo reporte':<22}{hora(respuesta.get('ts'))}")


def mostrar_historial(respuesta):
    total = int(respuesta.get("count", 0))
    print(f"Ultimos {total} reportes de {respuesta.get('node_id')} (mas reciente primero)")
    tabla(["HORA", "CPU %", "TEMP C", "POWER %", "LINK"], filas_reportes(respuesta, "h", total))


def mostrar_eventos(respuesta):
    total = int(respuesta.get("count", 0))
    filas = []
    for i in range(total):
        p = f"e{i}_"
        filas.append([hora(respuesta.get(p + "ts")), respuesta.get(p + "event"),
                      respuesta.get(p + "severity"), respuesta.get(p + "detail", "")])
    print(f"Ultimos {total} eventos de {respuesta.get('node_id')} (mas reciente primero)")
    tabla(["HORA", "EVENTO", "SEVERIDAD", "DETALLE"], filas)


def pedir_nodo(con_cantidad):
    campos = {"node_id": input("ID del nodo: ").strip()}
    if con_cantidad:
        cantidad = input("Cantidad (1-10) [5]: ").strip()
        if cantidad:
            campos["n"] = cantidad
    return campos


def monitor(sesion):
    try:
        while True:
            respuesta = sesion.consultar({"q": "LIST"})
            print("\033[2J\033[H", end="")
            print(f"Monitor en vivo - {time.strftime('%H:%M:%S')} (Ctrl+C para volver)\n")
            mostrar_lista(respuesta)
            time.sleep(INTERVALO_MONITOR)
    except KeyboardInterrupt:
        print()


MENU = """
=== Estacion de control - Operaciones ===
1) Estado de todos los nodos
2) Detalle de un nodo
3) Historial de un nodo
4) Eventos de un nodo
5) Monitor en vivo
0) Salir"""


def ejecutar_opcion(sesion, opcion):
    if opcion == "1":
        mostrar_lista(sesion.consultar({"q": "LIST"}))
    elif opcion == "2":
        mostrar_nodo(sesion.consultar({"q": "NODE"} | pedir_nodo(False)))
    elif opcion == "3":
        mostrar_historial(sesion.consultar({"q": "HISTORY"} | pedir_nodo(True)))
    elif opcion == "4":
        mostrar_eventos(sesion.consultar({"q": "EVENTS"} | pedir_nodo(True)))
    elif opcion == "5":
        monitor(sesion)
    else:
        print("Opcion invalida")


def main():
    parser = argparse.ArgumentParser(description="Cliente de operaciones SMCD")
    parser.add_argument("--host", required=True, help="nombre de dominio de la estacion de control")
    parser.add_argument("--puerto", required=True, type=int)
    args = parser.parse_args()

    sesion = Sesion(args.host, args.puerto)
    try:
        while True:
            print(MENU)
            opcion = input("Opcion: ").strip()
            if opcion == "0":
                break
            try:
                ejecutar_opcion(sesion, opcion)
            except ErrorServidor as e:
                print(f"Error del servidor: {e}")
            except ValueError as e:
                print(f"Dato invalido: {e}")
            except (OSError, smcd.ErrorProtocolo) as e:
                print(f"No se pudo comunicar con el servidor: {e}")
    except (KeyboardInterrupt, EOFError):
        print()
    finally:
        sesion.cerrar()


if __name__ == "__main__":
    main()
