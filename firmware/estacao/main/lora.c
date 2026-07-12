#include "lora.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_LORA_SCK GPIO_NUM_18
#define PIN_LORA_MISO GPIO_NUM_19
#define PIN_LORA_MOSI GPIO_NUM_23
#define PIN_LORA_NSS GPIO_NUM_5
#define PIN_LORA_RST GPIO_NUM_14

// 1 MHz: conservador de proposito — jumpers em protoboard degradam o
// sinal; o SX1278 aceita ate 10 MHz, subimos depois se tudo estiver ok.
#define LORA_SPI_HZ (1 * 1000 * 1000)

static const char *TAG = "lora";

static spi_device_handle_t s_spi;

// Reset por hardware (datasheet 7.2.2): RST em nivel baixo >= 100 us,
// solta, e o chip fica pronto em ~5 ms.
static void lora_reset(void) {
  gpio_set_level(PIN_LORA_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(2));
  gpio_set_level(PIN_LORA_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
}

esp_err_t lora_init(void) {
  gpio_config_t rst_cfg = {
      .pin_bit_mask = 1ULL << PIN_LORA_RST,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&rst_cfg));

  spi_bus_config_t bus_cfg = {
      .sclk_io_num = PIN_LORA_SCK,
      .miso_io_num = PIN_LORA_MISO,
      .mosi_io_num = PIN_LORA_MOSI,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

  // Pull-up de diagnostico no MISO: com a linha aberta (fio/NSS sem
  // contato) a leitura vira 0xFF; 0x00 persistente passa a apontar
  // para alimentacao do radio. Nao atrapalha a operacao normal.
  gpio_set_pull_mode(PIN_LORA_MISO, GPIO_PULLUP_ONLY);

  // O proprio driver conduz o NSS (abaixa na transacao, sobe no fim).
  spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = LORA_SPI_HZ,
      .mode = 0,  // SPI mode 0: clock repousa em baixo, amostra na subida
      .spics_io_num = PIN_LORA_NSS,
      .queue_size = 4,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &dev_cfg, &s_spi));

  lora_reset();
  ESP_LOGI(TAG, "SPI pronto (VSPI @ %d kHz), radio resetado", LORA_SPI_HZ / 1000);
  return ESP_OK;
}

esp_err_t lora_ler_reg(uint8_t reg, uint8_t *valor) {
  // Transacao de 2 bytes, full-duplex: manda [endereco | wnr=0] e um
  // byte qualquer; o registrador volta pelo MISO no segundo byte.
  uint8_t tx[2] = {reg & 0x7F, 0x00};
  uint8_t rx[2] = {0};
  spi_transaction_t t = {
      .length = 16,  // bits
      .tx_buffer = tx,
      .rx_buffer = rx,
  };
  esp_err_t err = spi_device_transmit(s_spi, &t);
  if (err == ESP_OK) *valor = rx[1];
  return err;
}
