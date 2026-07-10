#pragma once

#include "stddef.h"
#include "stdint.h"

// CRC16-CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF, sem
// reflexao. Vetor de teste universal: crc16_ccitt("123456789", 9)
// deve dar 0x29B1.
uint16_t crc16_ccitt(const uint8_t *dados, size_t tamanho);
