#pragma once

#include "stdbool.h"
#include "stdint.h"

// ============================================================
// Formato do pacote LoRa da estacao meteorologica (v1)
//
// CONTRATO DE FIO — compartilhado entre estacao (TX) e gateway (RX):
//   - layout exato dos 18 bytes abaixo, SEM padding (packed);
//   - inteiros multi-byte em LITTLE-ENDIAN (nativo do ESP32);
//   - ponto fixo: temp_x100 = temperatura em C * 100 (2643 = 26,43 C);
//   - crc16 = CRC16-CCITT-FALSE dos 16 bytes anteriores.
// Qualquer mudanca de layout = nova versao do formato.
// ============================================================

// Flags de erro: bit ligado = leitura daquele sensor falhou neste
// ciclo (campo correspondente vai zerado).
#define PACOTE_FLAG_ERRO_BMP280 (1 << 0)
#define PACOTE_FLAG_ERRO_LDR    (1 << 1)
#define PACOTE_FLAG_ERRO_CHUVA  (1 << 2)
#define PACOTE_FLAG_ERRO_DHT11  (1 << 3)
// bits 4-7 reservados

typedef struct __attribute__((packed)) {
  uint16_t seq;       // numero de sequencia (detecta pacote perdido)
  int16_t temp_x100;  // temperatura BMP280, C x100 (ponto fixo)
  uint32_t press_pa;  // pressao BMP280, Pa
  uint8_t umid;       // umidade relativa DHT11, %
  uint16_t luz_raw;   // LDR bruto 0-4095 (conversao fica p/ calibracao)
  uint16_t chuva_raw; // MH-RD AO bruto 0-4095 (menor = mais agua)
  uint16_t vento_ppm; // pulsos do reed por minuto (fator fisico: Fase 7)
  uint8_t flags;      // PACOTE_FLAG_ERRO_*
  uint16_t crc16;     // CRC16-CCITT dos 16 bytes anteriores
} pacote_t;

_Static_assert(sizeof(pacote_t) == 18,
               "pacote_t deve ter exatamente 18 bytes (formato de fio)");

// Calcula e grava o CRC (chamar depois de preencher os campos).
void pacote_finalizar(pacote_t *p);

// Confere o CRC (usar no receptor antes de confiar nos campos).
bool pacote_valido(const pacote_t *p);

// Auto-verificacao da implementacao do CRC (vetor conhecido 0x29B1).
// Chamar no boot; false = implementacao regrediu.
bool pacote_selfcheck(void);
