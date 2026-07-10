#include "pacote.h"

#include "crc16.h"
#include "string.h"

// O CRC cobre todos os bytes anteriores a ele proprio.
#define PACOTE_BYTES_COM_CRC (sizeof(pacote_t) - sizeof(uint16_t))

void pacote_finalizar(pacote_t *p) {
  p->crc16 = crc16_ccitt((const uint8_t *)p, PACOTE_BYTES_COM_CRC);
}

bool pacote_valido(const pacote_t *p) {
  return p->crc16 == crc16_ccitt((const uint8_t *)p, PACOTE_BYTES_COM_CRC);
}

bool pacote_selfcheck(void) {
  return crc16_ccitt((const uint8_t *)"123456789", 9) == 0x29B1;
}
