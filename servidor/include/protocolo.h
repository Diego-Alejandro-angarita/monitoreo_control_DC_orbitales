#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>
#include <stdint.h>

#include "smcd.h"

#define SMCD_MAX_PARES 32

typedef struct {
    uint8_t version;
    uint8_t tipo;
    uint16_t seq;
    uint16_t len;
} smcd_cabecera;

typedef struct {
    char clave[SMCD_MAX_CLAVE + 1];
    char valor[SMCD_MAX_VALOR + 1];
} smcd_par;

typedef struct {
    smcd_par pares[SMCD_MAX_PARES];
    int n;
} smcd_payload;

typedef struct {
    char datos[SMCD_MAX_PAYLOAD_TCP + 1];
    size_t len;
    int desbordado;
} smcd_buffer;

void cabecera_empaquetar(const smcd_cabecera *c, uint8_t *buf);
void cabecera_desempaquetar(const uint8_t *buf, smcd_cabecera *c);
size_t mensaje_armar(uint8_t *destino, uint8_t tipo, uint16_t seq, const smcd_buffer *b);

int payload_parsear(const char *datos, size_t len, smcd_payload *p);
const char *payload_obtener(const smcd_payload *p, const char *clave);
int payload_entero(const smcd_payload *p, const char *clave, long min, long max, long *valor);
int payload_decimal(const smcd_payload *p, const char *clave, double min, double max, double *valor);
int payload_opcion(const smcd_payload *p, const char *clave, const char *const opciones[]);

void buffer_iniciar(smcd_buffer *b);
void buffer_agregar(smcd_buffer *b, const char *clave, const char *formato, ...)
    __attribute__((format(printf, 3, 4)));
void buffer_error(smcd_buffer *b, int codigo, const char *mensaje, int ref_tipo);

int validar_node_id(const char *s);
int validar_entero(const char *s, long min, long max, long *valor);
int validar_decimal(const char *s, double min, double max, double *valor);
int validar_opcion(const char *s, const char *const opciones[]);

int seq_posterior(uint16_t a, uint16_t b);
int tipo_valido(int tipo);
const char *tipo_nombre(int tipo);

#endif
