#include "led_status.h"

#include "driver/ledc.h"
#include "esp_log.h"

// Silkscreen da placa: GPIO16=RX2, GPIO17=TX2 (UART2, nao usada).
#define LED_PIN_R GPIO_NUM_16
#define LED_PIN_G GPIO_NUM_17
#define LED_PIN_B GPIO_NUM_13

// 5 kHz: invisivel ao olho. 8 bits: duty 0-255, o RGB classico.
#define LED_FREQ_HZ 5000
#define LED_RESOLUCAO LEDC_TIMER_8_BIT

// Modulo Wcmcu sem resistores de corrente: teto de duty p/ limitar a
// corrente media no LED/GPIO. Com resistores de 220-330R, subir p/ 255.
#define LED_DUTY_MAX 64

static const char *TAG = "led_status";

static const ledc_channel_t s_canais[3] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1,
                                           LEDC_CHANNEL_2};
static const int s_pinos[3] = {LED_PIN_R, LED_PIN_G, LED_PIN_B};

esp_err_t led_status_init(void) {
  // Um timer da a frequencia; os 3 canais compartilham e variam o duty.
  ledc_timer_config_t timer_cfg = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = LED_FREQ_HZ,
      .duty_resolution = LED_RESOLUCAO,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  esp_err_t err = ledc_timer_config(&timer_cfg);
  if (err != ESP_OK) {
    return err;
  }

  for (int i = 0; i < 3; i++) {
    ledc_channel_config_t canal_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = s_canais[i],
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = s_pinos[i],
        .duty = 0, // comeca apagado
        .hpoint = 0,
        // Modulo Wcmcu e ANODO comum (apesar do silkscreen "-"): o pino
        // comum vai no 3V3 e a cor acende com nivel BAIXO. A inversao em
        // hardware mantem a semantica "duty = brilho" no resto do codigo.
        .flags.output_invert = 1,
    };
    err = ledc_channel_config(&canal_cfg);
    if (err != ESP_OK) {
      return err;
    }
  }
  ESP_LOGI(TAG, "LEDC pronto (R=%d G=%d B=%d, %d Hz)", LED_PIN_R, LED_PIN_G,
           LED_PIN_B, LED_FREQ_HZ);
  return ESP_OK;
}

void led_status_cor(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t cor[3] = {r, g, b};
  for (int i = 0; i < 3; i++) {
    uint32_t duty = (uint32_t)cor[i] * LED_DUTY_MAX / 255; // aplica o teto
    ledc_set_duty(LEDC_LOW_SPEED_MODE, s_canais[i], duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, s_canais[i]);
  }
}

void led_status_definir(led_status_t status) {
  switch (status) {
    case LED_STATUS_BOOT:        led_status_cor(0, 0, 128);   break;
    case LED_STATUS_OK:          led_status_cor(0, 128, 0);   break;
    case LED_STATUS_ERRO_SENSOR: led_status_cor(128, 100, 0); break;
    case LED_STATUS_FALHA:       led_status_cor(128, 0, 0);   break;
    case LED_STATUS_APAGADO:     led_status_cor(0, 0, 0);     break;
  }
}
