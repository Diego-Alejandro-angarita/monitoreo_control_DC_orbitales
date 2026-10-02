#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "protocolo.h"

static FILE *archivo;

int log_abrir(const char *ruta)
{
    archivo = fopen(ruta, "a");
    return archivo != NULL ? 0 : -1;
}

void log_cerrar(void)
{
    if (archivo != NULL)
        fclose(archivo);
    archivo = NULL;
}

static void escribir(const char *linea)
{
    char fecha[32];
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    strftime(fecha, sizeof fecha, "%Y-%m-%dT%H:%M:%S", &tm);

    printf("%s %s\n", fecha, linea);
    fflush(stdout);
    if (archivo != NULL) {
        fprintf(archivo, "%s %s\n", fecha, linea);
        fflush(archivo);
    }
}

void log_mensaje(const char *direccion, const char *transporte, const char *origen,
                 int tipo, uint16_t seq, const void *payload, size_t len)
{
    char linea[SMCD_MAX_PAYLOAD_TCP + 256];
    int n = snprintf(linea, sizeof linea, "%s %s %s %s seq=%u len=%zu ",
                     direccion, transporte, origen, tipo_nombre(tipo), seq, len);
    const unsigned char *bytes = payload;
    for (size_t i = 0; i < len && n < (int)sizeof linea - 1; i++)
        linea[n++] = (bytes[i] >= 0x20 && bytes[i] < 0x7F) ? (char)bytes[i] : '.';
    linea[n] = '\0';
    escribir(linea);
}

void log_info(const char *formato, ...)
{
    char linea[512] = "INFO ";
    va_list args;
    va_start(args, formato);
    vsnprintf(linea + 5, sizeof linea - 5, formato, args);
    va_end(args);
    escribir(linea);
}
