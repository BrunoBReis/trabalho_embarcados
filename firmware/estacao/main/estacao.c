// Dispatcher dos testes de bancada: o teste selecionado no menuconfig
// (Estacao - teste de bancada) decide o que o app_main executa.
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sensor_bmp280.h"

#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

static const char *TAG = "estacao";

static i2c_master_bus_handle_t iniciar_barramento_i2c(void) {
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
  return bus;
}

#if CONFIG_ESTACAO_TESTE_TODOS

// Autoteste de bancada: um veredito [SELFTEST] por componente.
// (Sera consolidado num modulo proprio no fim da Fase 2.)
static void autoteste(i2c_master_bus_handle_t bus) {
  esp_err_t err = sensor_bmp280_init(bus);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[SELFTEST] bmp280 OK");
  } else {
    ESP_LOGE(TAG, "[SELFTEST] bmp280 FALHA (%s)", esp_err_to_name(err));
  }
}

void app_main(void) {
  i2c_master_bus_handle_t bus = iniciar_barramento_i2c();
  autoteste(bus);

  while (true) {
    float temperatura_c = 0, pressao_hpa = 0;
    esp_err_t err = sensor_bmp280_ler(&temperatura_c, &pressao_hpa);
    if (err != ESP_OK) {
      // Falha de leitura nao derruba o sistema (principio da Fase 3).
      ESP_LOGE(TAG, "falha ao ler BMP280: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "temperatura: %.2f C | pressao: %.2f hPa", temperatura_c,
               pressao_hpa);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_TODOS

#if CONFIG_ESTACAO_TESTE_LDR

#include "sensor_ldr.h"

// Teste isolado do LDR: valor bruto a cada 1 s; cobrir/iluminar o
// sensor deve mover o numero de forma clara.
void app_main(void) {
  ESP_ERROR_CHECK(sensor_ldr_init());
  while (true) {
    int bruto = 0;
    esp_err_t err = sensor_ldr_ler(&bruto);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha ao ler LDR: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "luz (bruto 0-4095): %d", bruto);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_LDR

#if CONFIG_ESTACAO_TESTE_CHUVA

#include "sensor_chuva.h"

// Teste isolado do MH-RD: AO e DO lado a lado a cada 1 s. Molhar o
// pente faz o AO cair; o DO vira quando cruza o limiar do trimpot.
void app_main(void) {
  ESP_ERROR_CHECK(sensor_chuva_init());
  while (true) {
    int ao = 0;
    bool molhado = false;
    esp_err_t err = sensor_chuva_ler(&ao, &molhado);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha ao ler MH-RD: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "chuva AO (bruto 0-4095): %d | DO: %s", ao,
               molhado ? "MOLHADO" : "seco");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_CHUVA

#if CONFIG_ESTACAO_TESTE_DHT11

#include "sensor_dht11.h"

// Teste isolado do DHT11: leitura a cada 2 s (o sensor exige >= 1 s
// entre leituras). Falha ocasional de checksum e normal do protocolo.
void app_main(void) {
  ESP_ERROR_CHECK(sensor_dht11_init());
  while (true) {
    float umidade = 0, temperatura = 0;
    esp_err_t err = sensor_dht11_ler(&umidade, &temperatura);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha ao ler DHT11: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "umidade: %.0f %% | temperatura: %.1f C", umidade,
               temperatura);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_DHT11
