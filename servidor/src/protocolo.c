#include "protocolo.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char DESCONOCIDO[] = "DESCONOCIDO";

static const char *const NOMBRES[] = {
    [SMCD_REG_NODE] = "REG_NODE",
    [SMCD_REG_ACK] = "REG_ACK",
    [SMCD_STATE_REPORT] = "STATE_REPORT",
    [SMCD_EVENT] = "EVENT",
    [SMCD_EVENT_ACK] = "EVENT_ACK",
    [SMCD_AUTH_REQUEST] = "AUTH_REQUEST",
    [SMCD_AUTH_RESPONSE] = "AUTH_RESPONSE",
    [SMCD_QUERY] = "QUERY",
    [SMCD_QUERY_RESPONSE] = "QUERY_RESPONSE",
    [SMCD_ERROR] = "ERROR",
};

void cabecera_empaquetar(const smcd_cabecera *c, uint8_t *buf)
{
    uint16_t seq = htons(c->seq);
    uint16_t len = htons(c->len);
    buf[0] = c->version;
    buf[1] = c->tipo;
    memcpy(buf + 2, &seq, 2);
    memcpy(buf + 4, &len, 2);
}

void cabecera_desempaquetar(const uint8_t *buf, smcd_cabecera *c)
{
    uint16_t seq, len;
    memcpy(&seq, buf + 2, 2);
    memcpy(&len, buf + 4, 2);
    c->version = buf[0];
    c->tipo = buf[1];
    c->seq = ntohs(seq);
    c->len = ntohs(len);
}

size_t mensaje_armar(uint8_t *destino, uint8_t tipo, uint16_t seq, const smcd_buffer *b)
{
    smcd_cabecera c = {SMCD_VERSION, tipo, seq, (uint16_t)b->len};
    cabecera_empaquetar(&c, destino);
    memcpy(destino + SMCD_HEADER_LEN, b->datos, b->len);
    return SMCD_HEADER_LEN + b->len;
}

static int es_char_clave(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static int es_char_valor(char c)
{
    return c >= 0x20 && c <= 0x7E && c != ';' && c != '=';
}

int payload_parsear(const char *datos, size_t len, smcd_payload *p)
{
    size_t i = 0;
    p->n = 0;
    while (i < len) {
        size_t inicio = i;
        while (i < len && es_char_clave(datos[i]))
            i++;
        size_t largo_clave = i - inicio;
        if (i == len || datos[i] != '=' || largo_clave == 0
            || largo_clave > SMCD_MAX_CLAVE || p->n == SMCD_MAX_PARES)
            return -1;
        smcd_par *par = &p->pares[p->n];
        memcpy(par->clave, datos + inicio, largo_clave);
        par->clave[largo_clave] = '\0';

        inicio = ++i;
        while (i < len && es_char_valor(datos[i]))
            i++;
        size_t largo_valor = i - inicio;
        if (i == len || datos[i] != ';' || largo_valor > SMCD_MAX_VALOR)
            return -1;
        memcpy(par->valor, datos + inicio, largo_valor);
        par->valor[largo_valor] = '\0';
        i++;

        if (payload_obtener(p, par->clave) != NULL)
            return -1;
        p->n++;
    }
    return 0;
}

const char *payload_obtener(const smcd_payload *p, const char *clave)
{
    for (int i = 0; i < p->n; i++)
        if (strcmp(p->pares[i].clave, clave) == 0)
            return p->pares[i].valor;
    return NULL;
}

int payload_entero(const smcd_payload *p, const char *clave, long min, long max, long *valor)
{
    const char *s = payload_obtener(p, clave);
    return s != NULL && validar_entero(s, min, max, valor);
}

int payload_decimal(const smcd_payload *p, const char *clave, double min, double max, double *valor)
{
    const char *s = payload_obtener(p, clave);
    return s != NULL && validar_decimal(s, min, max, valor);
}

int payload_opcion(const smcd_payload *p, const char *clave, const char *const opciones[])
{
    const char *s = payload_obtener(p, clave);
    return s != NULL ? validar_opcion(s, opciones) : -1;
}

void buffer_iniciar(smcd_buffer *b)
{
    b->datos[0] = '\0';
    b->len = 0;
    b->desbordado = 0;
}

void buffer_agregar(smcd_buffer *b, const char *clave, const char *formato, ...)
{
    char valor[SMCD_MAX_VALOR + 1];
    va_list args;
    va_start(args, formato);
    vsnprintf(valor, sizeof valor, formato, args);
    va_end(args);

    size_t libre = sizeof b->datos - b->len;
    int n = snprintf(b->datos + b->len, libre, "%s=%s;", clave, valor);
    if (n < 0 || (size_t)n >= libre) {
        b->datos[b->len] = '\0';
        b->desbordado = 1;
        return;
    }
    b->len += (size_t)n;
}

void buffer_error(smcd_buffer *b, int codigo, const char *mensaje, int ref_tipo)
{
    buffer_iniciar(b);
    buffer_agregar(b, "code", "%d", codigo);
    buffer_agregar(b, "msg", "%s", mensaje);
    if (ref_tipo >= 0)
        buffer_agregar(b, "ref_type", "%d", ref_tipo);
}

int validar_node_id(const char *s)
{
    size_t len = strlen(s);
    if (len == 0 || len > SMCD_MAX_NODE_ID)
        return 0;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '-')
            return 0;
    return 1;
}

int validar_entero(const char *s, long min, long max, long *valor)
{
    char *fin;
    if (!isdigit((unsigned char)s[s[0] == '-']))
        return 0;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (errno != 0 || *fin != '\0' || v < min || v > max)
        return 0;
    *valor = v;
    return 1;
}

int validar_decimal(const char *s, double min, double max, double *valor)
{
    const char *c = s + (s[0] == '-');
    if (!isdigit((unsigned char)*c))
        return 0;
    while (isdigit((unsigned char)*c))
        c++;
    if (*c == '.') {
        c++;
        if (!isdigit((unsigned char)*c))
            return 0;
        while (isdigit((unsigned char)*c))
            c++;
    }
    if (*c != '\0')
        return 0;
    double v = strtod(s, NULL);
    if (v < min || v > max)
        return 0;
    *valor = v;
    return 1;
}

int validar_opcion(const char *s, const char *const opciones[])
{
    for (int i = 0; opciones[i] != NULL; i++)
        if (strcmp(s, opciones[i]) == 0)
            return i;
    return -1;
}

int seq_posterior(uint16_t a, uint16_t b)
{
    uint16_t diferencia = (uint16_t)(a - b);
    return diferencia != 0 && diferencia < 0x8000;
}

const char *tipo_nombre(int tipo)
{
    int total = (int)(sizeof NOMBRES / sizeof NOMBRES[0]);
    if (tipo < 0 || tipo >= total || NOMBRES[tipo] == NULL)
        return DESCONOCIDO;
    return NOMBRES[tipo];
}

int tipo_valido(int tipo)
{
    return tipo_nombre(tipo) != DESCONOCIDO;
}
