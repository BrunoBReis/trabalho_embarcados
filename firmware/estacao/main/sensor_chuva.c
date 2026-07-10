#include "sensor_chuva.h"

#include "adc1.h"
#include "driver/gpio.h"
#include "esp_log.h"

// GPIO 35 = ADC1_CH7 (input-only). DO no GPIO 27.
#define CHUVA_ADC_CANAL ADC_CHANNEL_7
#define CHUVA_PIN_DO GPIO_NUM_27
#define CHUVA_NUM_AMOSTRAS 16

static const char *TAG = "sensor_chuva";

static adc_oneshot_unit_handle_t s_adc = NULL;

esp_err_t sensor_chuva_init(void) {
  s_adc = adc1_obter(); // unidade compartilhada com os demais analogicos

  adc_oneshot_chan_cfg_t chan_cfg = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  esp_err_t err = adc_oneshot_config_channel(s_adc, CHUVA_ADC_CANAL, &chan_cfg);
  if (err != ESP_OK) {
    return err;
  }

  // DO: entrada digital comum (a plaquinha LM393 ja poe o nivel firme).
  gpio_config_t io_cfg = {
      .pin_bit_mask = 1ULL << CHUVA_PIN_DO,
      .mode = GPIO_MODE_INPUT,
  };
  err = gpio_config(&io_cfg);
  if (err != ESP_OK) {
    return err;
  }
  ESP_LOGI(TAG, "AO no ADC1 canal %d, DO no GPIO %d", CHUVA_ADC_CANAL,
           CHUVA_PIN_DO);
  return ESP_OK;
}

esp_err_t sensor_chuva_ler(int *ao_bruto, bool *molhado) {
  if (s_adc == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  int soma = 0;
  for (int i = 0; i < CHUVA_NUM_AMOSTRAS; i++) {
    int amostra = 0;
    esp_err_t err = adc_oneshot_read(s_adc, CHUVA_ADC_CANAL, &amostra);
    if (err != ESP_OK) {
      return err;
    }
    soma += amostra;
  }
  *ao_bruto = soma / CHUVA_NUM_AMOSTRAS;
  // LM393: nivel BAIXO quando a tensao do pente cruza o limiar = molhado.
  *molhado = (gpio_get_level(CHUVA_PIN_DO) == 0);
  return ESP_OK;
}
