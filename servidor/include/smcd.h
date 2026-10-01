#ifndef SMCD_H
#define SMCD_H

#define SMCD_VERSION 1
#define SMCD_HEADER_LEN 6
#define SMCD_MAX_PAYLOAD_UDP 1024
#define SMCD_MAX_PAYLOAD_TCP 8192

#define SMCD_MAX_CLAVE 32
#define SMCD_MAX_VALOR 128
#define SMCD_MAX_NODE_ID 16
#define SMCD_TOKEN_LEN 8

enum smcd_tipo {
    SMCD_REG_NODE       = 0x01,
    SMCD_REG_ACK        = 0x02,
    SMCD_STATE_REPORT   = 0x03,
    SMCD_EVENT          = 0x04,
    SMCD_EVENT_ACK      = 0x05,
    SMCD_AUTH_REQUEST   = 0x06,
    SMCD_AUTH_RESPONSE  = 0x07,
    SMCD_QUERY          = 0x08,
    SMCD_QUERY_RESPONSE = 0x09,
    SMCD_ERROR          = 0x0F
};

enum smcd_error {
    SMCD_ERR_MALFORMED         = 1,
    SMCD_ERR_BAD_VERSION       = 2,
    SMCD_ERR_UNKNOWN_TYPE      = 3,
    SMCD_ERR_INVALID_PARAM     = 4,
    SMCD_ERR_UNKNOWN_NODE      = 5,
    SMCD_ERR_NOT_AUTHENTICATED = 6,
    SMCD_ERR_FORBIDDEN         = 7,
    SMCD_ERR_TOO_LARGE         = 8,
    SMCD_ERR_WRONG_CHANNEL     = 9,
    SMCD_ERR_AUTH_UNAVAILABLE  = 10,
    SMCD_ERR_SERVER_BUSY       = 11,
    SMCD_ERR_INTERNAL          = 12
};

/* Temporizadores en segundos */
#define SMCD_T_REPORT          5
#define SMCD_FACTOR_INACTIVO   3
#define SMCD_T_ACK             2
#define SMCD_MAX_REINTENTOS    3
#define SMCD_T_REG             5
#define SMCD_T_REG_BACKOFF_MAX 30
#define SMCD_T_AUTH            30
#define SMCD_T_IDLE            300
#define SMCD_T_SERVICIO_AUTH   3
#define SMCD_T_RESPUESTA       5

#define SMCD_INTERVALO_MIN 1
#define SMCD_INTERVALO_MAX 60

#define SMCD_HIST_SIZE       10
#define SMCD_N_DEFECTO       5
#define SMCD_N_MAX           10
#define SMCD_VENTANA_EVENTOS 32
#define SMCD_MAX_NODOS       64
#define SMCD_MAX_CLIENTES    32
#define SMCD_MAX_AUTH_FALLOS 3

#endif
