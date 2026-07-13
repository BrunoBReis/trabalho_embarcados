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

// --- Registradores do SX1278 (datasheet, tabela 41; modo LoRa) ---
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06  // frequencia em 3 bytes, passo Fxosc/2^19
#define REG_PA_CONFIG 0x09
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE 0x0E
#define REG_IRQ_FLAGS 0x12
#define REG_MODEM_CONFIG1 0x1D
#define REG_MODEM_CONFIG2 0x1E
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG3 0x26

// RegOpMode: LongRangeMode (LoRa) + LowFrequencyModeOn (banda de 433).
// A troca pro modo LoRa so e aceita com o chip em SLEEP.
#define OPMODE_LORA_LF 0x88  // base: LoRa | LF | sleep
#define OPMODE_STANDBY (OPMODE_LORA_LF | 0x01)
#define OPMODE_TX (OPMODE_LORA_LF | 0x03)

#define IRQ_TX_DONE 0x08

// Potencia FIXA no minimo do PA_BOOST (unica saida ligada a antena no
// Ra-02): +2 dBm (~1,6 mW). Na bancada isso e proposital e importa:
// (1) inofensivo para o PA mesmo em carga ruim/aberta enquanto a
// cadeia de antena nao esta comprovada; (2) alcance de sobra ate o
// SDR. NAO subir sem antena validada e sem justificativa.
#define PA_CONFIG_BOOST_2DBM 0x80

// 433,0 MHz de proposito (longe dos controles de portao em 433,92):
// Frf = f / (32 MHz / 2^19) -> 433e6 * 2^19 / 32e6 = 0x6C4000.
#define FRF_433_0_MHZ 0x6C4000

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

// Escrita: mesmo formato da leitura, com o MSB do endereco em 1 (wnr).
static esp_err_t escrever_reg(uint8_t reg, uint8_t valor) {
  uint8_t tx[2] = {reg | 0x80, valor};
  spi_transaction_t t = {
      .length = 16,
      .tx_buffer = tx,
  };
  return spi_device_transmit(s_spi, &t);
}

// Escrita em rajada na FIFO: endereco uma vez, N bytes em seguida (o
// ponteiro interno avanca sozinho).
static esp_err_t escrever_fifo(const uint8_t *dados, uint8_t len) {
  uint8_t tx[33];
  tx[0] = REG_FIFO | 0x80;
  for (int i = 0; i < len; i++) tx[i + 1] = dados[i];
  spi_transaction_t t = {
      .length = (1 + len) * 8,
      .tx_buffer = tx,
  };
  return spi_device_transmit(s_spi, &t);
}

esp_err_t lora_config_modem(void) {
  // LoRa so pode ser ligado em SLEEP; depois tudo se configura em standby.
  ESP_ERROR_CHECK(escrever_reg(REG_OP_MODE, OPMODE_LORA_LF));
  vTaskDelay(pdMS_TO_TICKS(2));

  ESP_ERROR_CHECK(escrever_reg(REG_FRF_MSB + 0, (FRF_433_0_MHZ >> 16) & 0xFF));
  ESP_ERROR_CHECK(escrever_reg(REG_FRF_MSB + 1, (FRF_433_0_MHZ >> 8) & 0xFF));
  ESP_ERROR_CHECK(escrever_reg(REG_FRF_MSB + 2, FRF_433_0_MHZ & 0xFF));

  ESP_ERROR_CHECK(escrever_reg(REG_PA_CONFIG, PA_CONFIG_BOOST_2DBM));

  // BW 125 kHz (0111....), CR 4/5 (...001.), header explicito (.....0)
  ESP_ERROR_CHECK(escrever_reg(REG_MODEM_CONFIG1, 0x72));
  // SF12 (1100....), CRC do payload ligado (.....1..)
  ESP_ERROR_CHECK(escrever_reg(REG_MODEM_CONFIG2, 0xC4));
  // LowDataRateOptimize (obrigatorio: simbolo de 32,8 ms > 16 ms) + AGC
  ESP_ERROR_CHECK(escrever_reg(REG_MODEM_CONFIG3, 0x0C));

  ESP_ERROR_CHECK(escrever_reg(REG_OP_MODE, OPMODE_STANDBY));
  ESP_LOGI(TAG, "modem: 433.0 MHz, SF12, BW 125 kHz, CR 4/5, +2 dBm");
  return ESP_OK;
}

esp_err_t lora_tx(const uint8_t *dados, uint8_t len, uint32_t *ms_no_ar) {
  if (len == 0 || len > 32) return ESP_ERR_INVALID_ARG;

  ESP_ERROR_CHECK(escrever_reg(REG_OP_MODE, OPMODE_STANDBY));

  // Payload na FIFO: aponta o ponteiro para a base de TX e despeja.
  ESP_ERROR_CHECK(escrever_reg(REG_FIFO_TX_BASE, 0x00));
  ESP_ERROR_CHECK(escrever_reg(REG_FIFO_ADDR_PTR, 0x00));
  ESP_ERROR_CHECK(escrever_fifo(dados, len));
  ESP_ERROR_CHECK(escrever_reg(REG_PAYLOAD_LENGTH, len));

  // Limpa flags pendentes e dispara. O radio volta a standby sozinho
  // quando termina, deixando TxDone em RegIrqFlags.
  ESP_ERROR_CHECK(escrever_reg(REG_IRQ_FLAGS, 0xFF));
  ESP_ERROR_CHECK(escrever_reg(REG_OP_MODE, OPMODE_TX));

  TickType_t inicio = xTaskGetTickCount();
  while (true) {
    uint8_t irq = 0;
    ESP_ERROR_CHECK(lora_ler_reg(REG_IRQ_FLAGS, &irq));
    if (irq & IRQ_TX_DONE) break;
    if ((xTaskGetTickCount() - inicio) > pdMS_TO_TICKS(5000)) {
      ESP_LOGE(TAG, "timeout esperando TxDone (irq=0x%02X)", irq);
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_ERROR_CHECK(escrever_reg(REG_IRQ_FLAGS, 0xFF));

  if (ms_no_ar) {
    *ms_no_ar = (xTaskGetTickCount() - inicio) * portTICK_PERIOD_MS;
  }
  return ESP_OK;
}
