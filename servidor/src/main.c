#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "red.h"
#include "smcd.h"
#include "tcp_handler.h"
#include "udp_handler.h"

static int parsear_puerto(const char *texto)
{
    char *fin;
    errno = 0;
    long valor = strtol(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0' || valor < 1 || valor > 65535)
        return -1;
    return (int)valor;
}

static void aceptar(int tcp)
{
    struct sockaddr_storage direccion;
    socklen_t largo = sizeof direccion;
    int fd = accept(tcp, (struct sockaddr *)&direccion, &largo);
    if (fd < 0) {
        log_info("Error en accept: %s", strerror(errno));
        return;
    }
    char origen[RED_MAX_ORIGEN];
    red_origen((struct sockaddr *)&direccion, largo, origen);
    log_info("Conexion TCP aceptada desde %s", origen);
    tcp_atender(fd, origen);
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

    if (log_abrir(argv[2]) < 0) {
        fprintf(stderr, "No se pudo abrir el archivo de logs %s: %s\n", argv[2], strerror(errno));
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);

    int tcp = red_crear_servidor(puerto, SOCK_STREAM);
    int udp = red_crear_servidor(puerto, SOCK_DGRAM);
    if (tcp < 0 || udp < 0) {
        log_info("No se pudo abrir el puerto %d: %s", puerto, strerror(errno));
        log_cerrar();
        return EXIT_FAILURE;
    }
    log_info("Estacion de control SMCD v%d escuchando en el puerto %d (TCP y UDP)", SMCD_VERSION, puerto);

    for (;;) {
        fd_set listos;
        FD_ZERO(&listos);
        FD_SET(tcp, &listos);
        FD_SET(udp, &listos);
        if (select((tcp > udp ? tcp : udp) + 1, &listos, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            log_info("Error en select: %s", strerror(errno));
            break;
        }
        if (FD_ISSET(udp, &listos))
            udp_recibir(udp);
        if (FD_ISSET(tcp, &listos))
            aceptar(tcp);
    }

    close(tcp);
    close(udp);
    log_cerrar();
    return EXIT_FAILURE;
}
