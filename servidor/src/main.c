#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smcd.h"

static int parsear_puerto(const char *texto)
{
    char *fin;
    errno = 0;
    long valor = strtol(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0' || valor < 1 || valor > 65535)
        return -1;
    return (int)valor;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <puerto> <archivoDeLogs>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int puerto = parsear_puerto(argv[1]);
    if (puerto < 0) {
        fprintf(stderr, "Puerto inválido: %s (debe estar entre 1 y 65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    FILE *log = fopen(argv[2], "a");
    if (log == NULL) {
        fprintf(stderr, "No se pudo abrir el archivo de logs %s: %s\n", argv[2], strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Estación de control SMCD v%d - puerto %d - log %s\n", SMCD_VERSION, puerto, argv[2]);

    fclose(log);
    return EXIT_SUCCESS;
}
