#pragma once

#include "esp_err.h"

// Configura o ADC1 canal do LDR (GPIO 34, atenuacao 12 dB).
esp_err_t sensor_ldr_init(void);

// Le a luminosidade bruta 0-4095 (media de 16 amostras).
esp_err_t sensor_ldr_ler(int *bruto);
