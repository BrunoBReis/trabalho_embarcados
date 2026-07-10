#include "crc16.h"

// Implementacao bit a bit (didatica; ha versoes com tabela, mais
// rapidas, desnecessario para 18 bytes a cada 30 s). Para cada bit da
// mensagem: desloca o CRC e, se "vazou" um 1, aplica o polinomio (XOR).
// E a divisao polinomial em GF(2) do apendice de qualquer datasheet.
uint16_t crc16_ccitt(const uint8_t *dados, size_t tamanho) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < tamanho; i++) {
    crc ^= (uint16_t)dados[i] << 8;
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
