#include "tcp_handler.h"

#include <unistd.h>

#include "consultas.h"
#include "log.h"
#include "nodos.h"
#include "protocolo.h"
#include "red.h"

static void responder(int fd, const char *origen, uint8_t tipo, uint16_t seq, const smcd_buffer *b)
{
    uint8_t mensaje[SMCD_HEADER_LEN + SMCD_MAX_PAYLOAD_TCP];
    size_t len = mensaje_armar(mensaje, tipo, seq, b);
    if (red_send_todo(fd, mensaje, len) < 0)
        log_info("No se pudo enviar %s a %s", tipo_nombre(tipo), origen);
    log_mensaje("TX", "TCP", origen, tipo, seq, b->datos, b->len);
}

static void responder_error(int fd, const char *origen, uint16_t seq, int codigo,
                            const char *mensaje, int ref_tipo)
{
    smcd_buffer b;
    buffer_error(&b, codigo, mensaje, ref_tipo);
    responder(fd, origen, SMCD_ERROR, seq, &b);
}

static void registrar_nodo(int fd, const char *origen, uint16_t seq, const smcd_payload *p)
{
    const char *id = payload_obtener(p, "node_id");
    long intervalo;
    if (id == NULL || !validar_node_id(id)
        || !payload_entero(p, "intervalo", SMCD_INTERVALO_MIN, SMCD_INTERVALO_MAX, &intervalo)) {
        responder_error(fd, origen, seq, SMCD_ERR_INVALID_PARAM,
                        "node_id o intervalo ausente o invalido", SMCD_REG_NODE);
        return;
    }

    nodo *n = nodos_registrar(id, (int)intervalo);
    if (n == NULL) {
        responder_error(fd, origen, seq, SMCD_ERR_SERVER_BUSY, "tabla de nodos llena", SMCD_REG_NODE);
        return;
    }

    smcd_buffer b;
    buffer_iniciar(&b);
    buffer_agregar(&b, "status", "OK");
    buffer_agregar(&b, "token", "%s", n->token);
    buffer_agregar(&b, "intervalo", "%d", n->intervalo);
    buffer_agregar(&b, "timeout", "%d", n->intervalo * SMCD_FACTOR_INACTIVO);
    responder(fd, origen, SMCD_REG_ACK, seq, &b);
    log_info("Nodo %s registrado desde %s", n->id, origen);
}

static int procesar(int fd, const char *origen, const smcd_cabecera *c, const char *datos)
{
    if (!tipo_valido(c->tipo)) {
        responder_error(fd, origen, c->seq, SMCD_ERR_UNKNOWN_TYPE, "tipo de mensaje desconocido", c->tipo);
        return 1;
    }
    if (c->tipo == SMCD_ERROR)
        return 1;
    if (c->tipo != SMCD_REG_NODE && c->tipo != SMCD_AUTH_REQUEST && c->tipo != SMCD_QUERY) {
        responder_error(fd, origen, c->seq, SMCD_ERR_WRONG_CHANNEL, "tipo no aceptado por TCP", c->tipo);
        return 1;
    }

    smcd_payload p;
    if (payload_parsear(datos, c->len, &p) < 0) {
        responder_error(fd, origen, c->seq, SMCD_ERR_MALFORMED, "payload con formato invalido", c->tipo);
        return c->tipo != SMCD_REG_NODE;
    }

    if (c->tipo == SMCD_REG_NODE) {
        registrar_nodo(fd, origen, c->seq, &p);
        return 0;
    }
    if (c->tipo == SMCD_AUTH_REQUEST) {
        responder_error(fd, origen, c->seq, SMCD_ERR_AUTH_UNAVAILABLE,
                        "servicio de autenticacion no disponible", c->tipo);
        return 1;
    }

    smcd_buffer b;
    uint8_t tipo = consultas_atender(&p, &b);
    responder(fd, origen, tipo, c->seq, &b);
    return 1;
}

void tcp_atender(int fd, const char *origen)
{
    uint8_t cabecera[SMCD_HEADER_LEN];
    char datos[SMCD_MAX_PAYLOAD_TCP];

    for (;;) {
        int r = red_recv_exacto(fd, cabecera, sizeof cabecera);
        if (r == 0) {
            log_info("Conexion cerrada por %s", origen);
            break;
        }
        if (r < 0) {
            log_info("Conexion interrumpida con %s", origen);
            break;
        }

        smcd_cabecera c;
        cabecera_desempaquetar(cabecera, &c);
        if (c.version != SMCD_VERSION || c.len > SMCD_MAX_PAYLOAD_TCP) {
            log_mensaje("RX", "TCP", origen, c.tipo, c.seq, NULL, 0);
            if (c.version != SMCD_VERSION)
                responder_error(fd, origen, c.seq, SMCD_ERR_BAD_VERSION, "version no soportada", c.tipo);
            else
                responder_error(fd, origen, c.seq, SMCD_ERR_TOO_LARGE, "payload demasiado grande", c.tipo);
            break;
        }

        if (red_recv_exacto(fd, datos, c.len) != 1) {
            log_info("Payload incompleto de %s, se cierra la conexion", origen);
            break;
        }
        log_mensaje("RX", "TCP", origen, c.tipo, c.seq, datos, c.len);

        if (!procesar(fd, origen, &c, datos))
            break;
    }
    close(fd);
}
