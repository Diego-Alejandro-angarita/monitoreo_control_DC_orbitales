#include "nodos.h"

#include <stdio.h>
#include <string.h>
#include <sys/random.h>

#include "protocolo.h"

static nodo tabla[SMCD_MAX_NODOS];
static int num_nodos;

static time_t ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec;
}

static void generar_token(char *token)
{
    uint32_t valor;
    if (getrandom(&valor, sizeof valor, 0) != sizeof valor)
        valor = (uint32_t)time(NULL) ^ (uint32_t)clock();
    snprintf(token, SMCD_TOKEN_LEN + 1, "%08x", valor);
}

nodo *nodos_buscar(const char *id)
{
    for (int i = 0; i < num_nodos; i++)
        if (strcmp(tabla[i].id, id) == 0)
            return &tabla[i];
    return NULL;
}

nodo *nodos_registrar(const char *id, int intervalo)
{
    nodo *n = nodos_buscar(id);
    if (n == NULL) {
        if (num_nodos == SMCD_MAX_NODOS)
            return NULL;
        n = &tabla[num_nodos++];
        memset(n, 0, sizeof *n);
        snprintf(n->id, sizeof n->id, "%s", id);
    }
    generar_token(n->token);
    n->estado = NODO_ACTIVO;
    n->intervalo = intervalo;
    n->ultimo_contacto = ahora();
    n->hay_seq_reporte = 0;
    return n;
}

nodo *nodos_validar(const char *id, const char *token)
{
    nodo *n = nodos_buscar(id);
    if (n == NULL || n->estado != NODO_ACTIVO || strcmp(n->token, token) != 0)
        return NULL;
    return n;
}

int nodos_cantidad(void)
{
    return num_nodos;
}

nodo *nodos_en(int i)
{
    return &tabla[i];
}

int nodos_agregar_reporte(nodo *n, uint16_t seq, const reporte *r)
{
    n->ultimo_contacto = ahora();
    if (n->hay_seq_reporte) {
        if (!seq_posterior(seq, n->ultimo_seq_reporte))
            return 0;
        n->perdidos += (uint16_t)(seq - n->ultimo_seq_reporte - 1);
    }
    n->hay_seq_reporte = 1;
    n->ultimo_seq_reporte = seq;

    n->historial[n->pos_historial] = *r;
    n->pos_historial = (n->pos_historial + 1) % SMCD_HIST_SIZE;
    if (n->n_historial < SMCD_HIST_SIZE)
        n->n_historial++;
    n->reportes++;
    return 1;
}

void nodos_agregar_evento(nodo *n, const evento *e)
{
    n->ultimo_contacto = ahora();
    n->eventos[n->pos_eventos] = *e;
    n->pos_eventos = (n->pos_eventos + 1) % SMCD_HIST_SIZE;
    if (n->n_eventos < SMCD_HIST_SIZE)
        n->n_eventos++;
    n->total_eventos++;
}

const reporte *nodos_reporte(const nodo *n, int i)
{
    if (i >= n->n_historial)
        return NULL;
    return &n->historial[(n->pos_historial - 1 - i + SMCD_HIST_SIZE) % SMCD_HIST_SIZE];
}

const evento *nodos_evento(const nodo *n, int i)
{
    if (i >= n->n_eventos)
        return NULL;
    return &n->eventos[(n->pos_eventos - 1 - i + SMCD_HIST_SIZE) % SMCD_HIST_SIZE];
}

long nodos_sin_contacto(const nodo *n)
{
    return (long)(ahora() - n->ultimo_contacto);
}

const char *nodos_estado_texto(const nodo *n)
{
    return n->estado == NODO_ACTIVO ? "ACTIVO" : "INACTIVO";
}
