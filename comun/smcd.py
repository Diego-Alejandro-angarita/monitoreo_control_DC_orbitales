"""Codificación y decodificación de mensajes del protocolo SMCD."""

import re
import socket
import struct

VERSION = 1
CABECERA = struct.Struct("!BBHH")
MAX_PAYLOAD_UDP = 1024
MAX_PAYLOAD_TCP = 8192

REG_NODE = 0x01
REG_ACK = 0x02
STATE_REPORT = 0x03
EVENT = 0x04
EVENT_ACK = 0x05
AUTH_REQUEST = 0x06
AUTH_RESPONSE = 0x07
QUERY = 0x08
QUERY_RESPONSE = 0x09
ERROR = 0x0F

NOMBRES = {
    REG_NODE: "REG_NODE",
    REG_ACK: "REG_ACK",
    STATE_REPORT: "STATE_REPORT",
    EVENT: "EVENT",
    EVENT_ACK: "EVENT_ACK",
    AUTH_REQUEST: "AUTH_REQUEST",
    AUTH_RESPONSE: "AUTH_RESPONSE",
    QUERY: "QUERY",
    QUERY_RESPONSE: "QUERY_RESPONSE",
    ERROR: "ERROR",
}

ERRORES = {
    1: "MALFORMED",
    2: "BAD_VERSION",
    3: "UNKNOWN_TYPE",
    4: "INVALID_PARAM",
    5: "UNKNOWN_NODE",
    6: "NOT_AUTHENTICATED",
    7: "FORBIDDEN",
    8: "TOO_LARGE",
    9: "WRONG_CHANNEL",
    10: "AUTH_UNAVAILABLE",
    11: "SERVER_BUSY",
    12: "INTERNAL",
}
ERR_UNKNOWN_NODE = 5

T_REPORT = 5
T_ACK = 2
MAX_REINTENTOS = 3
T_REG = 5
T_REG_BACKOFF_MAX = 30
T_RESPUESTA = 5

CLAVE_VALIDA = re.compile(r"[A-Za-z0-9_]{1,32}")
VALOR_VALIDO = re.compile(r"[\x20-\x3a\x3c\x3e-\x7e]{0,128}")


class ErrorProtocolo(Exception):
    pass


def codificar_payload(campos):
    partes = []
    for clave, valor in campos.items():
        valor = str(valor)
        if not CLAVE_VALIDA.fullmatch(clave) or not VALOR_VALIDO.fullmatch(valor):
            raise ValueError(f"par invalido: {clave}={valor}")
        partes.append(f"{clave}={valor};")
    return "".join(partes).encode("ascii")


def decodificar_payload(datos):
    try:
        texto = datos.decode("ascii")
    except UnicodeDecodeError:
        raise ErrorProtocolo("payload no ASCII")
    if texto and not texto.endswith(";"):
        raise ErrorProtocolo("payload sin ';' final")
    campos = {}
    for par in texto.split(";")[:-1]:
        clave, separador, valor = par.partition("=")
        if not separador or not CLAVE_VALIDA.fullmatch(clave) or not VALOR_VALIDO.fullmatch(valor):
            raise ErrorProtocolo(f"par invalido: {par!r}")
        if clave in campos:
            raise ErrorProtocolo(f"clave repetida: {clave}")
        campos[clave] = valor
    return campos


def armar(tipo, seq, campos=None, maximo=MAX_PAYLOAD_TCP):
    payload = codificar_payload(campos or {})
    if len(payload) > maximo:
        raise ValueError("payload demasiado grande")
    return CABECERA.pack(VERSION, tipo, seq, len(payload)) + payload


def leer_cabecera(datos):
    version, tipo, seq, largo = CABECERA.unpack(datos[:CABECERA.size])
    if version != VERSION:
        raise ErrorProtocolo(f"version no soportada: {version}")
    return tipo, seq, largo


def desarmar(datagrama):
    if len(datagrama) < CABECERA.size:
        raise ErrorProtocolo("datagrama menor que la cabecera")
    tipo, seq, largo = leer_cabecera(datagrama)
    if largo != len(datagrama) - CABECERA.size:
        raise ErrorProtocolo("payload_len no coincide con el datagrama")
    return tipo, seq, decodificar_payload(datagrama[CABECERA.size:])


def recv_exacto(sock, n):
    datos = b""
    while len(datos) < n:
        bloque = sock.recv(n - len(datos))
        if not bloque:
            raise ConnectionError("conexion cerrada por el servidor")
        datos += bloque
    return datos


def recibir_tcp(sock):
    tipo, seq, largo = leer_cabecera(recv_exacto(sock, CABECERA.size))
    return tipo, seq, decodificar_payload(recv_exacto(sock, largo))


def resolver(host, puerto, tipo_socket):
    return socket.getaddrinfo(host, puerto, type=tipo_socket)


def conectar_tcp(host, puerto, timeout):
    ultimo_error = OSError(f"sin direcciones para {host}")
    for familia, tipo, protocolo, _, direccion in resolver(host, puerto, socket.SOCK_STREAM):
        sock = socket.socket(familia, tipo, protocolo)
        sock.settimeout(timeout)
        try:
            sock.connect(direccion)
            return sock
        except OSError as e:
            sock.close()
            ultimo_error = e
    raise ultimo_error


def siguiente_seq(seq):
    return (seq + 1) & 0xFFFF


def describir_error(campos):
    codigo = campos.get("code", "?")
    nombre = ERRORES.get(int(codigo), "DESCONOCIDO") if codigo.isdigit() else "DESCONOCIDO"
    return f"{nombre} ({codigo}): {campos.get('msg', '')}"
