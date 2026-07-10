#pragma once

#include "esp_err.h"
#include "stdbool.h"

// Configura AO (GPIO 35, ADC1_CH7) e DO (GPIO 27) do MH-RD.
esp_err_t sensor_chuva_init(void);

// ao_bruto: 0-4095 (media de 16 amostras; MENOR = mais agua).
// molhado: leitura do comparador DO (limiar do trimpot da plaquinha).
esp_err_t sensor_chuva_ler(int *ao_bruto, bool *molhado);
