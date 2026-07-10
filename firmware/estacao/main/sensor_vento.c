#include "sensor_vento.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "stdatomic.h"

#define VENTO_PIN GPIO_NUM_25
// Pulsos a menos de 10 ms do anterior sao bounce mecanico das laminas
// do reed, nao volta do ima.
#define VENTO_DEBOUNCE_US 10000

static const char *TAG = "sensor_vento";

// Escrito pela ISR, lido/zerado pela tarefa -> atomico.
static atomic_uint_fast32_t s_pulsos = 0;
static int64_t s_ultimo_pulso_us = 0;

// Regras de ISR: curta, sem log/bloqueio, so APIs seguras em interrupcao.
static void IRAM_ATTR vento_isr(void *arg) {
  int64_t agora = esp_timer_get_time();
  if (agora - s_ultimo_pulso_us >= VENTO_DEBOUNCE_US) {
    atomic_fetch_add(&s_pulsos, 1);
    s_ultimo_pulso_us = agora;
  }
}

esp_err_t sensor_vento_init(void) {
  gpio_config_t io_cfg = {
      .pin_bit_mask = 1ULL << VENTO_PIN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE, // aberto = 1; ima fecha p/ GND = 0
      .intr_type = GPIO_INTR_NEGEDGE,   // evento na borda de descida
  };
  esp_err_t err = gpio_config(&io_cfg);
  if (err != ESP_OK) {
    return err;
  }

  // Servico de despacho de ISRs de GPIO (uma vez por firmware); se um
  // outro modulo ja instalou, ESP_ERR_INVALID_STATE nao e problema.
  err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }
  err = gpio_isr_handler_add(VENTO_PIN, vento_isr, NULL);
  if (err != ESP_OK) {
    return err;
  }
  ESP_LOGI(TAG, "reed no GPIO %d (pull-up, borda de descida)", VENTO_PIN);
  return ESP_OK;
}

uint32_t sensor_vento_coletar_pulsos(void) {
  // Troca atomica por zero: nenhum pulso se perde entre ler e zerar.
  return (uint32_t)atomic_exchange(&s_pulsos, 0);
}
