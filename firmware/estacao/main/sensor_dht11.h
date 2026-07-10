#pragma once

#include "esp_err.h"

// Configura o pino de dados do DHT11 (GPIO 4).
esp_err_t sensor_dht11_init(void);

// Le umidade relativa (%) e temperatura (C). Leituras a no maximo
// 1x por segundo (limite do proprio sensor); falhas de checksum sao
// normais ocasionalmente — tratar como erro transitorio.
esp_err_t sensor_dht11_ler(float *umidade_pct, float *temperatura_c);
