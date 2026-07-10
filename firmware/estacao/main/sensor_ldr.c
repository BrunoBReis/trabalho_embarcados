#include "sensor_ldr.h"

#include "adc1.h"
#include "esp_log.h"

// GPIO 34 = ADC1_CH6 (input-only; ADC1 nao conflita com Wi-Fi).
#define LDR_ADC_CANAL ADC_CHANNEL_6
#define LDR_NUM_AMOSTRAS 16

static const char *TAG = "sensor_ldr";

static adc_oneshot_unit_handle_t s_adc = NULL;

esp_err_t sensor_ldr_init(void) {
  s_adc = adc1_obter(); // unidade compartilhada com os demais analogicos

  adc_oneshot_chan_cfg_t chan_cfg = {
      // 12 dB: faixa de entrada ate ~3.1 V (o divisor do KY-018 usa 3V3).
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12, // 0-4095
  };
  esp_err_t err = adc_oneshot_config_channel(s_adc, LDR_ADC_CANAL, &chan_cfg);
  if (err != ESP_OK) {
    return err;
  }
  ESP_LOGI(TAG, "ADC1 canal %d configurado (12 dB, 12 bits)", LDR_ADC_CANAL);
  return ESP_OK;
}

esp_err_t sensor_ldr_ler(int *bruto) {
  if (s_adc == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  // Media de N amostras: o ruido aleatorio do ADC se cancela na soma.
  int soma = 0;
  for (int i = 0; i < LDR_NUM_AMOSTRAS; i++) {
    int amostra = 0;
    esp_err_t err = adc_oneshot_read(s_adc, LDR_ADC_CANAL, &amostra);
    if (err != ESP_OK) {
      return err;
    }
    soma += amostra;
  }
  *bruto = soma / LDR_NUM_AMOSTRAS;
  return ESP_OK;
}
