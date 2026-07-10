#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

// Detecta (0x76/0x77), inicializa e configura o BMP280 no barramento dado.
esp_err_t sensor_bmp280_init(i2c_master_bus_handle_t bus);

// Le temperatura (C) e pressao (hPa) compensadas.
esp_err_t sensor_bmp280_ler(float *temperatura_c, float *pressao_hpa);
