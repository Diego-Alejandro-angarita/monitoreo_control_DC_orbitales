#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

int log_abrir(const char *ruta);
void log_cerrar(void);
void log_mensaje(const char *direccion, const char *transporte, const char *origen,
                 int tipo, uint16_t seq, const void *payload, size_t len);
void log_info(const char *formato, ...) __attribute__((format(printf, 1, 2)));

#endif
