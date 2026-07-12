#pragma once

#include <stdint.h>

#include "esp_err.h"

// Driver minimo do radio LoRa Ra-02 (SX1278) via SPI.
// Fase 4, passo 1: so o bring-up do barramento — reset por hardware e
// leitura de registradores. TX/RX entram nos passos seguintes.

// Registradores do SX1278 usados ate aqui (datasheet, tabela 41).
#define LORA_REG_VERSION 0x42  // valor fixo em silicio: 0x12

// Inicializa o SPI (VSPI: SCK 18, MISO 19, MOSI 23, NSS 5) e da o
// pulso de reset no radio (RST 14). Nao transmite nada.
esp_err_t lora_init(void);

// Le um registrador de 8 bits do SX1278.
esp_err_t lora_ler_reg(uint8_t reg, uint8_t *valor);
