#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

// Datasheet BMP280 (Bosch), secao 4.3.1: registrador "id" em 0xD0
// contem o valor fixo 0x58 (0x60 indicaria um BME280).
#define BMP280_REG_ID 0xD0
#define BMP280_CHIP_ID 0x58

static const char *TAG = "estacao";

// O endereco do BMP280 depende do pino SDO do modulo:
// SDO->GND = 0x76, SDO->VCC = 0x77. Sondamos os dois.
static uint8_t bmp280_detectar_endereco(i2c_master_bus_handle_t bus) {
  const uint8_t candidatos[] = {0x76, 0x77};
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
    return; // encerra o app_main; a tarefa main termina aqui
  }
  ESP_LOGI(TAG, "BMP280 detectado em 0x%02X", endereco);

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = endereco,
      .scl_speed_hz = 100000, // 100 kHz (modo standard)
  };
  i2c_master_dev_handle_t dev;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

  while (true) {
    uint8_t reg = BMP280_REG_ID;
    uint8_t id = 0;
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, &id, 1, 100);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha na leitura I2C: %s", esp_err_to_name(err));
    } else if (id == BMP280_CHIP_ID) {
      ESP_LOGI(TAG, "chip id = 0x%02X (BMP280 confirmado)", id);
    } else {
      ESP_LOGW(TAG, "chip id = 0x%02X (esperava 0x58 — chip diferente?)", id);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
