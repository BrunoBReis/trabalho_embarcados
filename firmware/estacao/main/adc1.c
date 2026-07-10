#include "adc1.h"

#include "esp_log.h"

static const char *TAG = "adc1";

static adc_oneshot_unit_handle_t s_adc1 = NULL;

adc_oneshot_unit_handle_t adc1_obter(void) {
  if (s_adc1 == NULL) {
    adc_oneshot_unit_init_cfg_t cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&cfg, &s_adc1));
    ESP_LOGI(TAG, "unidade ADC1 criada");
  }
  return s_adc1;
}
