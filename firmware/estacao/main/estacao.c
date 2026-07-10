#include "bmp280.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

static const char *TAG = "estacao";

// O endereco do BMP280 depende do pino SDO do modulo:
// SDO->GND = 0x76, SDO->VCC = 0x77. Sondamos os dois.
static uint8_t bmp280_detectar_endereco(i2c_master_bus_handle_t bus) {
  const uint8_t candidatos[] = {I2C_BMP280_DEV_ADDR_LO, I2C_BMP280_DEV_ADDR_HI};
  for (int i = 0; i < 2; i++) {
    if (i2c_master_probe(bus, candidatos[i], 50) == ESP_OK) {
      return candidatos[i];
    }
  }
  return 0; // ninguem respondeu
}

void app_main(void) {
  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = PIN_I2C_SDA,
      .scl_io_num = PIN_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_master_bus_handle_t bus;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

  uint8_t endereco = bmp280_detectar_endereco(bus);
  if (endereco == 0) {
    ESP_LOGE(TAG, "BMP280 nao encontrado em 0x76/0x77 — conferir fiacao");
    return;
  }
  ESP_LOGI(TAG, "BMP280 detectado em 0x%02X", endereco);

  bmp280_config_t cfg = I2C_BMP280_CONFIG_DEFAULT;
  cfg.i2c_address = endereco;
  bmp280_handle_t bmp280;
  ESP_ERROR_CHECK(bmp280_init(bus, &cfg, &bmp280));
  ESP_LOGI(TAG, "BMP280 inicializado (calibracao lida, modo normal)");

  while (true) {
    float temperatura_c = 0;
    float pressao_pa = 0;
    esp_err_t err = bmp280_get_measurements(bmp280, &temperatura_c, &pressao_pa);
    if (err != ESP_OK) {
      // Falha de leitura nao derruba o sistema (principio da Fase 3).
      ESP_LOGE(TAG, "falha ao ler BMP280: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "temperatura: %.2f C | pressao: %.2f hPa", temperatura_c,
               pressao_pa / 100.0f);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
