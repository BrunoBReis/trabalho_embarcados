#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

static const char *TAG = "estacao";

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

  while (true) {
    ESP_LOGI(TAG, "varrendo o barramento I2C (0x08-0x77)...");
    int encontrados = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
      if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
        ESP_LOGI(TAG, "dispositivo respondeu (ACK) em 0x%02X", addr);
        encontrados++;
      }
    }
    if (encontrados == 0) {
      ESP_LOGW(TAG, "nenhum dispositivo encontrado — conferir fiacao/pull-ups");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
