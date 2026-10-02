#include "consultas.h"

#include <stdio.h>

#include "nodos.h"

enum { Q_LIST, Q_NODE, Q_HISTORY, Q_EVENTS };

static const char *const CONSULTAS[] = {"LIST", "NODE", "HISTORY", "EVENTS", NULL};

static const char *clave(char *destino, const char *prefijo, const char *nombre)
{
    snprintf(destino, SMCD_MAX_CLAVE + 1, "%s%s", prefijo, nombre);
    return destino;
}

static void agregar_reporte(smcd_buffer *b, const char *prefijo, const reporte *r)
{
    char k[SMCD_MAX_CLAVE + 1];
    buffer_agregar(b, clave(k, prefijo, "ts"), "%ld", r->ts);
    buffer_agregar(b, clave(k, prefijo, "cpu"), "%.1f", r->cpu);
    buffer_agregar(b, clave(k, prefijo, "temp"), "%.1f", r->temp);
    buffer_agregar(b, clave(k, prefijo, "power"), "%.1f", r->power);
    buffer_agregar(b, clave(k, prefijo, "link"), "%s", r->link);
}

static void listar(smcd_buffer *b)
{
    char k[SMCD_MAX_CLAVE + 1], prefijo[16];
    int total = nodos_cantidad();
    buffer_agregar(b, "count", "%d", total);
    for (int i = 0; i < total; i++) {
        const nodo *n = nodos_en(i);
        snprintf(prefijo, sizeof prefijo, "n%d_", i);
        buffer_agregar(b, clave(k, prefijo, "id"), "%s", n->id);
        buffer_agregar(b, clave(k, prefijo, "estado"), "%s", nodos_estado_texto(n));
        const reporte *r = nodos_reporte(n, 0);
        if (r != NULL)
            agregar_reporte(b, prefijo, r);
    }
}

static void detallar(smcd_buffer *b, const nodo *n)
{
    buffer_agregar(b, "node_id", "%s", n->id);
    buffer_agregar(b, "estado", "%s", nodos_estado_texto(n));
    buffer_agregar(b, "intervalo", "%d", n->intervalo);
    buffer_agregar(b, "ultimo_visto", "%ld", nodos_sin_contacto(n));
    buffer_agregar(b, "reportes", "%lu", n->reportes);
    buffer_agregar(b, "perdidos", "%lu", n->perdidos);
    buffer_agregar(b, "eventos", "%lu", n->total_eventos);
    const reporte *r = nodos_reporte(n, 0);
    if (r != NULL)
        agregar_reporte(b, "", r);
}

static void historial(smcd_buffer *b, const nodo *n, int cantidad)
{
    char prefijo[16];
    int total = cantidad < n->n_historial ? cantidad : n->n_historial;
    buffer_agregar(b, "node_id", "%s", n->id);
    buffer_agregar(b, "count", "%d", total);
    for (int i = 0; i < total; i++) {
        snprintf(prefijo, sizeof prefijo, "h%d_", i);
        agregar_reporte(b, prefijo, nodos_reporte(n, i));
    }
}

static void eventos(smcd_buffer *b, const nodo *n, int cantidad)
{
    char k[SMCD_MAX_CLAVE + 1], prefijo[16];
    int total = cantidad < n->n_eventos ? cantidad : n->n_eventos;
    buffer_agregar(b, "node_id", "%s", n->id);
    buffer_agregar(b, "count", "%d", total);
    for (int i = 0; i < total; i++) {
        const evento *e = nodos_evento(n, i);
        snprintf(prefijo, sizeof prefijo, "e%d_", i);
        buffer_agregar(b, clave(k, prefijo, "ts"), "%ld", e->ts);
        buffer_agregar(b, clave(k, prefijo, "event"), "%s", e->tipo);
        buffer_agregar(b, clave(k, prefijo, "severity"), "%s", e->severidad);
        if (e->detalle[0] != '\0')
            buffer_agregar(b, clave(k, prefijo, "detail"), "%s", e->detalle);
    }
}

static uint8_t error(smcd_buffer *b, int codigo, const char *mensaje)
{
    buffer_error(b, codigo, mensaje, SMCD_QUERY);
    return SMCD_ERROR;
}

uint8_t consultas_atender(const smcd_payload *p, smcd_buffer *b)
{
    int consulta = payload_opcion(p, "q", CONSULTAS);
    if (consulta < 0)
        return error(b, SMCD_ERR_INVALID_PARAM, "consulta q ausente o desconocida");

    buffer_iniciar(b);
    buffer_agregar(b, "status", "OK");
    if (consulta == Q_LIST) {
        listar(b);
        return SMCD_QUERY_RESPONSE;
    }

    const char *id = payload_obtener(p, "node_id");
    if (id == NULL || !validar_node_id(id))
        return error(b, SMCD_ERR_INVALID_PARAM, "node_id ausente o invalido");

    long cantidad = SMCD_N_DEFECTO;
    if (payload_obtener(p, "n") != NULL && consulta != Q_NODE
        && !payload_entero(p, "n", 1, SMCD_N_MAX, &cantidad))
        return error(b, SMCD_ERR_INVALID_PARAM, "n fuera de rango");

    const nodo *n = nodos_buscar(id);
    if (n == NULL)
        return error(b, SMCD_ERR_UNKNOWN_NODE, "nodo desconocido");

    if (consulta == Q_NODE)
        detallar(b, n);
    else if (consulta == Q_HISTORY)
        historial(b, n, (int)cantidad);
    else
        eventos(b, n, (int)cantidad);
    return SMCD_QUERY_RESPONSE;
}
