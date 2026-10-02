#!/usr/bin/env python3
"""Nodo orbital simulado: se registra en la estación de control y reporta telemetría."""

import argparse
import random
import re
import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "comun"))
import smcd  # noqa: E402

SEVERIDADES = {
    "OVERHEAT": "CRITICAL",
    "LOW_POWER": "WARNING",
    "LINK_LOST": "CRITICAL",
    "HARDWARE_FAULT": "CRITICAL",
}
PROB_FALLA_HARDWARE = 0.01


class ErrorRegistro(Exception):
    pass


def limitar(valor, minimo, maximo):
    return max(minimo, min(maximo, valor))


class Telemetria:
    def __init__(self):
        self.cpu = random.uniform(20, 60)
        self.temp = random.uniform(30, 45)
        self.power = random.uniform(70, 100)
        self.link = "UP"

    def avanzar(self):
        self.cpu = limitar(self.cpu + random.uniform(-8, 8), 0, 100)
        self.temp = limitar(self.temp + random.uniform(-3, 3) + (40 - self.temp) * 0.1
                            + (self.cpu - 50) * 0.1, -50, 150)
        self.power = limitar(self.power + random.uniform(-6, 6) + (60 - self.power) * 0.05, 0, 100)
        self.link = random.choices(["UP", "DEGRADED", "DOWN"], weights=[90, 7, 3])[0]

    def campos(self):
        return {
            "cpu": f"{self.cpu:.1f}",
            "temp_int": f"{self.temp:.1f}",
            "power": f"{self.power:.1f}",
            "link": self.link,
        }

    def condiciones(self):
        return {
            "OVERHEAT": (self.temp > 85, f"temp_int {self.temp:.1f}"),
            "LOW_POWER": (self.power < 15, f"power {self.power:.1f}"),
            "LINK_LOST": (self.link == "DOWN", "enlace caido"),
        }


class Nodo:
    def __init__(self, args):
        self.id = args.id
        self.host = args.host
        self.puerto = args.puerto
        self.intervalo = args.intervalo
        self.tipo = args.tipo
        self.token = None
        self.udp = None
        self.servidor_udp = None
        self.seq_registro = 0
        self.seq_reporte = 0
        self.seq_evento = 0
        self.telemetria = Telemetria()
        self.condiciones_activas = set()

    def log(self, texto):
        print(f"{time.strftime('%H:%M:%S')} [{self.id}] {texto}", flush=True)

    def registrar(self):
        espera = 1
        while True:
            try:
                self.intentar_registro()
                return
            except (OSError, smcd.ErrorProtocolo, ErrorRegistro) as e:
                self.log(f"Registro fallido: {e}. Reintento en {espera} s")
                time.sleep(espera)
                espera = min(espera * 2, smcd.T_REG_BACKOFF_MAX)

    def intentar_registro(self):
        self.seq_registro = smcd.siguiente_seq(self.seq_registro)
        campos = {"node_id": self.id, "intervalo": self.intervalo, "tipo": self.tipo}
        with smcd.conectar_tcp(self.host, self.puerto, smcd.T_REG) as sock:
            sock.sendall(smcd.armar(smcd.REG_NODE, self.seq_registro, campos))
            tipo, _, respuesta = smcd.recibir_tcp(sock)
        if tipo == smcd.ERROR:
            raise ErrorRegistro(smcd.describir_error(respuesta))
        if tipo != smcd.REG_ACK or "token" not in respuesta:
            raise ErrorRegistro("respuesta inesperada del servidor")

        familia, tipo_socket, protocolo, _, direccion = smcd.resolver(
            self.host, self.puerto, socket.SOCK_DGRAM)[0]
        if self.udp is not None:
            self.udp.close()
        self.udp = socket.socket(familia, tipo_socket, protocolo)
        self.servidor_udp = direccion
        self.token = respuesta["token"]
        self.intervalo = int(respuesta.get("intervalo", self.intervalo))
        self.log(f"Registrado. token={self.token} intervalo={self.intervalo} s")

    def enviar(self, tipo, seq, campos):
        datos = smcd.armar(tipo, seq, campos, smcd.MAX_PAYLOAD_UDP)
        try:
            self.udp.sendto(datos, self.servidor_udp)
        except OSError as e:
            self.log(f"No se pudo enviar {smcd.NOMBRES[tipo]}: {e}")

    def identidad(self):
        return {"node_id": self.id, "token": self.token, "ts": int(time.time())}

    def enviar_reporte(self):
        self.telemetria.avanzar()
        self.seq_reporte = smcd.siguiente_seq(self.seq_reporte)
        campos = self.identidad() | self.telemetria.campos()
        self.enviar(smcd.STATE_REPORT, self.seq_reporte, campos)
        t = self.telemetria
        self.log(f"STATE_REPORT seq={self.seq_reporte} cpu={t.cpu:.1f} temp={t.temp:.1f} "
                 f"power={t.power:.1f} link={t.link}")

    def enviar_evento(self, evento, detalle):
        self.seq_evento = smcd.siguiente_seq(self.seq_evento)
        campos = self.identidad() | {"event": evento, "severity": SEVERIDADES[evento], "detail": detalle}
        self.enviar(smcd.EVENT, self.seq_evento, campos)
        self.log(f"EVENT seq={self.seq_evento} {evento} ({detalle})")

    def revisar_eventos(self):
        activas = set()
        for evento, (activa, detalle) in self.telemetria.condiciones().items():
            if activa:
                activas.add(evento)
                if evento not in self.condiciones_activas:
                    self.enviar_evento(evento, detalle)
        self.condiciones_activas = activas
        if random.random() < PROB_FALLA_HARDWARE:
            self.enviar_evento("HARDWARE_FAULT", "falla simulada de hardware")

    def procesar(self, datos):
        try:
            tipo, seq, campos = smcd.desarmar(datos)
        except smcd.ErrorProtocolo as e:
            self.log(f"Mensaje invalido descartado: {e}")
            return
        if tipo == smcd.EVENT_ACK:
            self.log(f"EVENT_ACK seq={seq} status={campos.get('status')}")
        elif tipo == smcd.ERROR:
            self.log(f"ERROR del servidor seq={seq}: {smcd.describir_error(campos)}")
            if campos.get("code") == str(smcd.ERR_UNKNOWN_NODE):
                self.log("El servidor no reconoce el nodo. Estado INACTIVO, se re-registra")
                self.token = None
        else:
            self.log(f"Mensaje inesperado descartado: {smcd.NOMBRES.get(tipo, tipo)}")

    def ejecutar(self):
        proximo = 0.0
        while True:
            if self.token is None:
                self.registrar()
                proximo = 0.0
            if time.monotonic() >= proximo:
                self.enviar_reporte()
                self.revisar_eventos()
                proximo = time.monotonic() + self.intervalo
            self.udp.settimeout(max(0.05, proximo - time.monotonic()))
            try:
                datos, _ = self.udp.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError as e:
                self.log(f"Error de recepcion UDP: {e}")
                continue
            self.procesar(datos)


def entero_en_rango(minimo, maximo):
    def convertir(texto):
        valor = int(texto)
        if not minimo <= valor <= maximo:
            raise argparse.ArgumentTypeError(f"debe estar entre {minimo} y {maximo}")
        return valor
    return convertir


def id_nodo(texto):
    if not re.fullmatch(r"[A-Za-z0-9-]{1,16}", texto):
        raise argparse.ArgumentTypeError("solo letras, digitos y '-', maximo 16 caracteres")
    return texto


def main():
    parser = argparse.ArgumentParser(description="Nodo orbital SMCD")
    parser.add_argument("--host", required=True, help="nombre de dominio de la estacion de control")
    parser.add_argument("--puerto", required=True, type=entero_en_rango(1, 65535))
    parser.add_argument("--id", required=True, type=id_nodo)
    parser.add_argument("--intervalo", type=entero_en_rango(1, 60), default=smcd.T_REPORT)
    parser.add_argument("--tipo", default="LEO", help="tipo de orbita")
    nodo = Nodo(parser.parse_args())
    try:
        nodo.ejecutar()
    except KeyboardInterrupt:
        nodo.log("Nodo detenido")


if __name__ == "__main__":
    main()
