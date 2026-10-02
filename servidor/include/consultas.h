#ifndef CONSULTAS_H
#define CONSULTAS_H

#include <stdint.h>

#include "protocolo.h"

uint8_t consultas_atender(const smcd_payload *p, smcd_buffer *b);

#endif
