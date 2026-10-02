#ifndef NODOS_H
#define NODOS_H

#include <stdint.h>
#include <time.h>

#include "smcd.h"

typedef struct {
    long ts;
    double cpu;
    double temp;
    double power;
    char link[9];
} reporte;

typedef struct {
    long ts;
    char tipo[16];
    char severidad[9];
    char detalle[SMCD_MAX_VALOR + 1];
} evento;

typedef enum { NODO_ACTIVO, NODO_INACTIVO } estado_nodo;

typedef struct {
    char id[SMCD_MAX_NODE_ID + 1];
    char token[SMCD_TOKEN_LEN + 1];
    estado_nodo estado;
    int intervalo;
    time_t ultimo_contacto;
    int hay_seq_reporte;
    uint16_t ultimo_seq_reporte;
    reporte historial[SMCD_HIST_SIZE];
    int n_historial;
    int pos_historial;
    evento eventos[SMCD_HIST_SIZE];
    int n_eventos;
    int pos_eventos;
    unsigned long reportes;
    unsigned long perdidos;
    unsigned long total_eventos;
} nodo;

nodo *nodos_registrar(const char *id, int intervalo);
nodo *nodos_buscar(const char *id);
nodo *nodos_validar(const char *id, const char *token);
int nodos_cantidad(void);
nodo *nodos_en(int i);

int nodos_agregar_reporte(nodo *n, uint16_t seq, const reporte *r);
void nodos_agregar_evento(nodo *n, const evento *e);
const reporte *nodos_reporte(const nodo *n, int i);
const evento *nodos_evento(const nodo *n, int i);

long nodos_sin_contacto(const nodo *n);
const char *nodos_estado_texto(const nodo *n);

#endif
