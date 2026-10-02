#include "udp_handler.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "nodos.h"
#include "protocolo.h"
#include "red.h"

typedef struct {
    int fd;
    const struct sockaddr *direccion;
    socklen_t largo;
    const char *origen;
} remitente;

static const char *const LINKS[] = {"UP", "DEGRADED", "DOWN", NULL};
static const char *const EVENTOS[] = {"OVERHEAT", "LOW_POWER", "LINK_LOST", "HARDWARE_FAULT", NULL};
static const char *const SEVERIDADES[] = {"WARNING", "CRITICAL", NULL};

static void responder(const remitente *r, uint8_t tipo, uint16_t seq, const smcd_buffer *b)
{
    uint8_t mensaje[SMCD_HEADER_LEN + SMCD_MAX_PAYLOAD_TCP];
    size_t len = mensaje_armar(mensaje, tipo, seq, b);
    if (sendto(r->fd, mensaje, len, 0, r->direccion, r->largo) < 0)
        log_info("No se pudo enviar %s a %s: %s", tipo_nombre(tipo), r->origen, strerror(errno));
    log_mensaje("TX", "UDP", r->origen, tipo, seq, b->datos, b->len);
}

static void responder_error(const remitente *r, uint16_t seq, int codigo, const char *mensaje, int ref_tipo)
{
    smcd_buffer b;
    buffer_error(&b, codigo, mensaje, ref_tipo);
    responder(r, SMCD_ERROR, seq, &b);
}

static void campo_invalido(const remitente *r, uint16_t seq, const char *campo, int ref_tipo)
{
    char mensaje[64];
    snprintf(mensaje, sizeof mensaje, "campo %s ausente o fuera de rango", campo);
    responder_error(r, seq, SMCD_ERR_INVALID_PARAM, mensaje, ref_tipo);
}

static nodo *nodo_remitente(const remitente *r, uint16_t seq, const smcd_payload *p, int ref_tipo)
{
    nodo *n = nodos_validar(payload_obtener(p, "node_id"), payload_obtener(p, "token"));
    if (n == NULL)
        responder_error(r, seq, SMCD_ERR_UNKNOWN_NODE, "nodo no registrado o token invalido", ref_tipo);
    return n;
}

static int identidad_valida(const smcd_payload *p)
{
    const char *id = payload_obtener(p, "node_id");
    return id != NULL && validar_node_id(id) && payload_obtener(p, "token") != NULL;
}

static void procesar_reporte(const remitente *r, uint16_t seq, const smcd_payload *p)
{
    reporte rep;
    int link = -1;
    const char *invalido = NULL;
    if (!identidad_valida(p))
        invalido = "node_id o token";
    else if (!payload_entero(p, "ts", 0, LONG_MAX, &rep.ts))
        invalido = "ts";
    else if (!payload_decimal(p, "cpu", 0, 100, &rep.cpu))
        invalido = "cpu";
    else if (!payload_decimal(p, "temp_int", -50, 150, &rep.temp))
        invalido = "temp_int";
    else if (!payload_decimal(p, "power", 0, 100, &rep.power))
        invalido = "power";
    else if ((link = payload_opcion(p, "link", LINKS)) < 0)
        invalido = "link";
    if (invalido != NULL) {
        campo_invalido(r, seq, invalido, SMCD_STATE_REPORT);
        return;
    }

    nodo *n = nodo_remitente(r, seq, p, SMCD_STATE_REPORT);
    if (n == NULL)
        return;
    snprintf(rep.link, sizeof rep.link, "%s", LINKS[link]);
    if (!nodos_agregar_reporte(n, seq, &rep))
        log_info("Reporte seq=%u de %s descartado por atrasado o duplicado", seq, n->id);
}

static void procesar_evento(const remitente *r, uint16_t seq, const smcd_payload *p)
{
    evento ev;
    int tipo = -1, severidad = -1;
    const char *invalido = NULL;
    if (!identidad_valida(p))
        invalido = "node_id o token";
    else if (!payload_entero(p, "ts", 0, LONG_MAX, &ev.ts))
        invalido = "ts";
    else if ((tipo = payload_opcion(p, "event", EVENTOS)) < 0)
        invalido = "event";
    else if ((severidad = payload_opcion(p, "severity", SEVERIDADES)) < 0)
        invalido = "severity";
    if (invalido != NULL) {
        campo_invalido(r, seq, invalido, SMCD_EVENT);
        return;
    }

    nodo *n = nodo_remitente(r, seq, p, SMCD_EVENT);
    if (n == NULL)
        return;
    const char *detalle = payload_obtener(p, "detail");
    snprintf(ev.tipo, sizeof ev.tipo, "%s", EVENTOS[tipo]);
    snprintf(ev.severidad, sizeof ev.severidad, "%s", SEVERIDADES[severidad]);
    snprintf(ev.detalle, sizeof ev.detalle, "%s", detalle != NULL ? detalle : "");
    nodos_agregar_evento(n, &ev);
    log_info("Evento %s (%s) del nodo %s", ev.tipo, ev.severidad, n->id);

    smcd_buffer b;
    buffer_iniciar(&b);
    buffer_agregar(&b, "status", "OK");
    responder(r, SMCD_EVENT_ACK, seq, &b);
}

static void procesar(const remitente *r, const uint8_t *buf, size_t len)
{
    if (len < SMCD_HEADER_LEN) {
        log_mensaje("RX", "UDP", r->origen, -1, 0, buf, len);
        responder_error(r, 0, SMCD_ERR_MALFORMED, "datagrama menor que la cabecera", -1);
        return;
    }

    smcd_cabecera c;
    cabecera_desempaquetar(buf, &c);
    const char *datos = (const char *)buf + SMCD_HEADER_LEN;
    size_t recibidos = len - SMCD_HEADER_LEN;
    log_mensaje("RX", "UDP", r->origen, c.tipo, c.seq, datos, recibidos);

    smcd_payload p;
    if (c.version != SMCD_VERSION)
        responder_error(r, c.seq, SMCD_ERR_BAD_VERSION, "version no soportada", c.tipo);
    else if (c.len > SMCD_MAX_PAYLOAD_UDP)
        responder_error(r, c.seq, SMCD_ERR_TOO_LARGE, "payload demasiado grande", c.tipo);
    else if (c.len != recibidos)
        responder_error(r, c.seq, SMCD_ERR_MALFORMED, "payload_len no coincide con el datagrama", c.tipo);
    else if (!tipo_valido(c.tipo))
        responder_error(r, c.seq, SMCD_ERR_UNKNOWN_TYPE, "tipo de mensaje desconocido", c.tipo);
    else if (c.tipo == SMCD_ERROR)
        return;
    else if (c.tipo != SMCD_STATE_REPORT && c.tipo != SMCD_EVENT)
        responder_error(r, c.seq, SMCD_ERR_WRONG_CHANNEL, "tipo no aceptado por UDP", c.tipo);
    else if (payload_parsear(datos, c.len, &p) < 0)
        responder_error(r, c.seq, SMCD_ERR_MALFORMED, "payload con formato invalido", c.tipo);
    else if (c.tipo == SMCD_STATE_REPORT)
        procesar_reporte(r, c.seq, &p);
    else
        procesar_evento(r, c.seq, &p);
}

void udp_recibir(int fd)
{
    uint8_t buf[65536];
    struct sockaddr_storage direccion;
    socklen_t largo = sizeof direccion;
    ssize_t len = recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&direccion, &largo);
    if (len < 0) {
        if (errno != EINTR)
            log_info("Error en recvfrom: %s", strerror(errno));
        return;
    }

    char origen[RED_MAX_ORIGEN];
    red_origen((struct sockaddr *)&direccion, largo, origen);
    remitente r = {fd, (struct sockaddr *)&direccion, largo, origen};
    procesar(&r, buf, (size_t)len);
}
