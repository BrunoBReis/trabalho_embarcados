#include "sensor_bmp280.h"

#include "bmp280.h"
#include "esp_log.h"

static const char *TAG = "sensor_bmp280";

static bmp280_handle_t s_bmp280 = NULL;

// O endereco depende do pino SDO do modulo: GND=0x76, VCC=0x77.
static uint8_t detectar_endereco(i2c_master_bus_handle_t bus) {
  const uint8_t candidatos[] = {I2C_BMP280_DEV_ADDR_LO, I2C_BMP280_DEV_ADDR_HI};
  for (int i = 0; i < 2; i++) {
    if (i2c_master_probe(bus, candidatos[i], 50) == ESP_OK) {
      return candidatos[i];
    }
  }
  return 0;
}

esp_err_t sensor_bmp280_init(i2c_master_bus_handle_t bus) {
  uint8_t endereco = detectar_endereco(bus);
  if (endereco == 0) {
    ESP_LOGE(TAG, "nao encontrado em 0x76/0x77");
    return ESP_ERR_NOT_FOUND;
  }
  ESP_LOGI(TAG, "detectado em 0x%02X", endereco);

  bmp280_config_t cfg = I2C_BMP280_CONFIG_DEFAULT;
  cfg.i2c_address = endereco;
  return bmp280_init(bus, &cfg, &s_bmp280);
}

esp_err_t sensor_bmp280_ler(float *temperatura_c, float *pressao_hpa) {
  if (s_bmp280 == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  float pressao_pa = 0;
  esp_err_t err = bmp280_get_measurements(s_bmp280, temperatura_c, &pressao_pa);
  if (err == ESP_OK) {
    *pressao_hpa = pressao_pa / 100.0f;
  }
  return err;
}
