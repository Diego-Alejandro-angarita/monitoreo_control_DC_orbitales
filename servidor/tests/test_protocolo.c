#include <stdio.h>
#include <string.h>

#include "protocolo.h"

static int fallos;

#define VERIFICAR(cond)                                                    \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            fallos++;                                                      \
        }                                                                  \
    } while (0)

static int parsear(const char *texto, smcd_payload *p)
{
    return payload_parsear(texto, strlen(texto), p);
}

static void probar_cabecera(void)
{
    const uint8_t esperado[] = {0x01, 0x01, 0x00, 0x01, 0x00, 0x33};
    smcd_cabecera c = {1, SMCD_REG_NODE, 1, 51}, leida;
    uint8_t buf[SMCD_HEADER_LEN];
    cabecera_empaquetar(&c, buf);
    VERIFICAR(memcmp(buf, esperado, sizeof esperado) == 0);

    const uint8_t reporte[] = {0x01, 0x03, 0x00, 0x2a, 0x00, 0x56};
    cabecera_desempaquetar(reporte, &leida);
    VERIFICAR(leida.version == 1 && leida.tipo == SMCD_STATE_REPORT);
    VERIFICAR(leida.seq == 42 && leida.len == 86);
}

static void probar_payload(void)
{
    smcd_payload p;
    VERIFICAR(parsear("node_id=SAT-01;intervalo=5;", &p) == 0);
    VERIFICAR(p.n == 2);
    VERIFICAR(strcmp(payload_obtener(&p, "node_id"), "SAT-01") == 0);
    VERIFICAR(payload_obtener(&p, "token") == NULL);
    VERIFICAR(parsear("", &p) == 0 && p.n == 0);
    VERIFICAR(parsear("detail=temp_int 92.1;vacio=;", &p) == 0);

    VERIFICAR(parsear("node_id=SAT-01", &p) < 0);
    VERIFICAR(parsear("a=1;a=2;", &p) < 0);
    VERIFICAR(parsear("a=b=c;", &p) < 0);
    VERIFICAR(parsear("=1;", &p) < 0);
    VERIFICAR(parsear("a=1;;", &p) < 0);
    VERIFICAR(parsear("clave-guion=1;", &p) < 0);
    VERIFICAR(payload_parsear("a=1\0;", 5, &p) < 0);

    char largo[200];
    snprintf(largo, sizeof largo, "%s=1;", "abcdefghijabcdefghijabcdefghijabc");
    VERIFICAR(parsear(largo, &p) < 0);

    char muchos[400] = "";
    for (int i = 0; i <= SMCD_MAX_PARES; i++) {
        char par[16];
        snprintf(par, sizeof par, "k%d=1;", i);
        strcat(muchos, par);
    }
    VERIFICAR(parsear(muchos, &p) < 0);
}

static void probar_validaciones(void)
{
    long entero;
    double decimal;
    VERIFICAR(validar_entero("15", 1, 60, &entero) && entero == 15);
    VERIFICAR(!validar_entero("0", 1, 60, &entero));
    VERIFICAR(!validar_entero("5a", 1, 60, &entero));
    VERIFICAR(!validar_entero("+5", 1, 60, &entero));
    VERIFICAR(!validar_entero("", 1, 60, &entero));

    VERIFICAR(validar_decimal("45.2", 0, 100, &decimal) && decimal > 45.1 && decimal < 45.3);
    VERIFICAR(validar_decimal("-50", -50, 150, &decimal));
    VERIFICAR(!validar_decimal("150.5", -50, 150, &decimal));
    VERIFICAR(!validar_decimal("1e2", 0, 1000, &decimal));
    VERIFICAR(!validar_decimal("abc", 0, 100, &decimal));
    VERIFICAR(!validar_decimal("4.", 0, 100, &decimal));
    VERIFICAR(!validar_decimal(".5", 0, 100, &decimal));

    VERIFICAR(validar_node_id("SAT-01"));
    VERIFICAR(!validar_node_id(""));
    VERIFICAR(!validar_node_id("SAT_01"));
    VERIFICAR(!validar_node_id("ABCDEFGHIJKLMNOPQ"));

    const char *const opciones[] = {"UP", "DOWN", NULL};
    VERIFICAR(validar_opcion("DOWN", opciones) == 1);
    VERIFICAR(validar_opcion("up", opciones) == -1);
}

static void probar_secuencias(void)
{
    VERIFICAR(seq_posterior(2, 1));
    VERIFICAR(!seq_posterior(1, 2));
    VERIFICAR(!seq_posterior(7, 7));
    VERIFICAR(seq_posterior(0, 65535));
    VERIFICAR(!seq_posterior(65535, 0));
}

static void probar_buffer(void)
{
    smcd_buffer b;
    buffer_error(&b, SMCD_ERR_INVALID_PARAM, "cpu fuera de rango", SMCD_STATE_REPORT);
    VERIFICAR(strcmp(b.datos, "code=4;msg=cpu fuera de rango;ref_type=3;") == 0);
    VERIFICAR(b.len == 41 && !b.desbordado);

    buffer_iniciar(&b);
    for (int i = 0; i < 2000; i++)
        buffer_agregar(&b, "clave", "%d", i);
    VERIFICAR(b.desbordado && b.len <= SMCD_MAX_PAYLOAD_TCP);

    uint8_t mensaje[SMCD_HEADER_LEN + SMCD_MAX_PAYLOAD_TCP];
    buffer_iniciar(&b);
    buffer_agregar(&b, "status", "OK");
    VERIFICAR(mensaje_armar(mensaje, SMCD_EVENT_ACK, 7, &b) == 16);
    VERIFICAR(mensaje[1] == SMCD_EVENT_ACK && mensaje[3] == 7 && mensaje[5] == 10);
}

static void probar_tipos(void)
{
    VERIFICAR(tipo_valido(SMCD_ERROR));
    VERIFICAR(!tipo_valido(0x0A));
    VERIFICAR(!tipo_valido(0));
    VERIFICAR(!tipo_valido(-1));
    VERIFICAR(strcmp(tipo_nombre(SMCD_QUERY), "QUERY") == 0);
}

int main(void)
{
    probar_cabecera();
    probar_payload();
    probar_validaciones();
    probar_secuencias();
    probar_buffer();
    probar_tipos();
    if (fallos == 0)
        printf("Todas las pruebas de protocolo pasaron\n");
    return fallos != 0;
}
