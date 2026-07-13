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

// Bloco compartilhado entre o modo estacao e o modo bancada.
#if CONFIG_ESTACAO_TESTE_TODOS || CONFIG_ESTACAO_MODO_ESTACAO

#include "led_status.h"
#include "pacote.h"
#include "sensor_chuva.h"
#include "sensor_dht11.h"
#include "sensor_ldr.h"
#include "sensor_vento.h"

// Um veredito por componente, em formato estavel para o tools/bancada.py.
// Retorna true se OK.
static bool selftest(const char *nome, esp_err_t err) {
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[SELFTEST] %s OK", nome);
    return true;
  }
  ESP_LOGE(TAG, "[SELFTEST] %s FALHA (%s)", nome, esp_err_to_name(err));
  return false;
}

// Autoteste de bancada. bmp280/dht11 comprovam comunicacao real; nos
// analogicos (ldr/chuva) e no reed o ADC/GPIO nao distingue "ausente"
// de "escuro/seco/parado" — para esses o OK atesta inicializacao.
static int autoteste(i2c_master_bus_handle_t bus) {
  int falhas = 0;

  falhas += !selftest("led", led_status_init());
  led_status_definir(LED_STATUS_BOOT);

  // CRC do pacote LoRa contra vetor conhecido (regressao acusa no boot).
  falhas += !selftest("pacote", pacote_selfcheck() ? ESP_OK : ESP_FAIL);

  falhas += !selftest("bmp280", sensor_bmp280_init(bus));
  falhas += !selftest("ldr", sensor_ldr_init());
  falhas += !selftest("chuva", sensor_chuva_init());
  falhas += !selftest("vento", sensor_vento_init());

  // DHT11: a primeira leitura pos-boot falha as vezes; 3 tentativas.
  esp_err_t err_dht = ESP_FAIL;
  for (int i = 0; i < 3 && err_dht != ESP_OK; i++) {
    float u = 0, t = 0;
    vTaskDelay(pdMS_TO_TICKS(2000));
    err_dht = sensor_dht11_ler(&u, &t);
  }
  falhas += !selftest("dht11", err_dht);

  ESP_LOGI(TAG, "[SELFTEST] resultado: %d falha(s)", falhas);
  led_status_definir(falhas == 0 ? LED_STATUS_OK : LED_STATUS_ERRO_SENSOR);
  return falhas;
}

#endif // bloco compartilhado

#if CONFIG_ESTACAO_TESTE_TODOS

void app_main(void) {
  i2c_master_bus_handle_t bus = iniciar_barramento_i2c();
  int falhas_boot = autoteste(bus);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Falha de leitura marca erro mas nao derruba o loop (Fase 3).
    bool erro_ciclo = false;

    float t_bmp = 0, p_hpa = 0;
    if (sensor_bmp280_ler(&t_bmp, &p_hpa) != ESP_OK) erro_ciclo = true;

    int luz = -1;
    if (sensor_ldr_ler(&luz) != ESP_OK) erro_ciclo = true;

    int chuva_ao = -1;
    bool molhado = false;
    if (sensor_chuva_ler(&chuva_ao, &molhado) != ESP_OK) erro_ciclo = true;

    float umid = 0, t_dht = 0;
    if (sensor_dht11_ler(&umid, &t_dht) != ESP_OK) erro_ciclo = true;

    uint32_t pulsos = sensor_vento_coletar_pulsos();

    ESP_LOGI(TAG,
             "T=%.2fC P=%.1fhPa | luz=%d | chuva=%d(%s) | umid=%.0f%% | "
             "vento=%lu pulsos/5s",
             t_bmp, p_hpa, luz, chuva_ao, molhado ? "MOLHADO" : "seco", umid,
             (unsigned long)pulsos);

    led_status_definir((erro_ciclo || falhas_boot > 0) ? LED_STATUS_ERRO_SENSOR
                                                       : LED_STATUS_OK);
  }
}

#endif // CONFIG_ESTACAO_TESTE_TODOS

#if CONFIG_ESTACAO_MODO_ESTACAO

#include "lora.h"

// Firmware da estacao: a cada intervalo, le todos os sensores, monta o
// pacote binario (formato de common/pacote), imprime legivel + hex e o
// TRANSMITE por LoRa. Falha de sensor vira bit em flags e campo zerado;
// radio ausente nao derruba o loop (fica so o log + LED de falha).
void app_main(void) {
  i2c_master_bus_handle_t bus = iniciar_barramento_i2c();
  int falhas_boot = autoteste(bus);
  if (falhas_boot > 0) {
    ESP_LOGW(TAG, "iniciando com %d componente(s) em falha", falhas_boot);
  }

  bool lora_ok = false;
  if (lora_init() == ESP_OK) {
    uint8_t versao = 0;
    if (lora_ler_reg(LORA_REG_VERSION, &versao) == ESP_OK && versao == 0x12) {
      ESP_ERROR_CHECK(lora_config_modem());
      lora_ok = true;
    }
  }
  if (!selftest("lora", lora_ok ? ESP_OK : ESP_ERR_TIMEOUT)) {
    ESP_LOGE(TAG, "radio mudo: pacotes ficarao so no log serial");
  }

  static pacote_t pacote = {0};

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(CONFIG_ESTACAO_INTERVALO_S * 1000));

    pacote.seq++;
    pacote.flags = 0;

    float t_bmp = 0, p_hpa = 0;
    if (sensor_bmp280_ler(&t_bmp, &p_hpa) == ESP_OK) {
      pacote.temp_x100 = (int16_t)(t_bmp * 100.0f);   // ponto fixo
      pacote.press_pa = (uint32_t)(p_hpa * 100.0f);   // hPa -> Pa
    } else {
      pacote.temp_x100 = 0;
      pacote.press_pa = 0;
      pacote.flags |= PACOTE_FLAG_ERRO_BMP280;
    }

    int luz = 0;
    if (sensor_ldr_ler(&luz) == ESP_OK) {
      pacote.luz_raw = (uint16_t)luz;
    } else {
      pacote.luz_raw = 0;
      pacote.flags |= PACOTE_FLAG_ERRO_LDR;
    }

    int chuva_ao = 0;
    bool molhado = false;
    if (sensor_chuva_ler(&chuva_ao, &molhado) == ESP_OK) {
      pacote.chuva_raw = (uint16_t)chuva_ao;
    } else {
      pacote.chuva_raw = 0;
      pacote.flags |= PACOTE_FLAG_ERRO_CHUVA;
    }

    float umid = 0, t_dht = 0;
    if (sensor_dht11_ler(&umid, &t_dht) == ESP_OK) {
      pacote.umid = (uint8_t)umid;
    } else {
      pacote.umid = 0;
      pacote.flags |= PACOTE_FLAG_ERRO_DHT11;
    }

    uint32_t pulsos = sensor_vento_coletar_pulsos();
    pacote.vento_ppm = (uint16_t)(pulsos * 60 / CONFIG_ESTACAO_INTERVALO_S);

    pacote_finalizar(&pacote);

    ESP_LOGI(TAG,
             "pacote seq=%u: T=%.2fC P=%luPa umid=%u%% luz=%u chuva=%u "
             "vento=%u ppm flags=0x%02X crc=0x%04X",
             pacote.seq, pacote.temp_x100 / 100.0f,
             (unsigned long)pacote.press_pa, pacote.umid, pacote.luz_raw,
             pacote.chuva_raw, pacote.vento_ppm, pacote.flags, pacote.crc16);
    ESP_LOG_BUFFER_HEX(TAG, &pacote, sizeof(pacote));

    bool tx_falhou = false;
    if (lora_ok) {
      uint32_t ms = 0;
      if (lora_tx((const uint8_t *)&pacote, sizeof(pacote), &ms) == ESP_OK) {
        ESP_LOGI(TAG, "LoRa TX ok: %u bytes, %lu ms no ar",
                 (unsigned)sizeof(pacote), (unsigned long)ms);
      } else {
        tx_falhou = true;
        ESP_LOGE(TAG, "LoRa TX falhou");
      }
    }

    led_status_definir((!lora_ok || tx_falhou) ? LED_STATUS_FALHA
                       : pacote.flags == 0     ? LED_STATUS_OK
                                               : LED_STATUS_ERRO_SENSOR);
  }
}

#endif // CONFIG_ESTACAO_MODO_ESTACAO

#if CONFIG_ESTACAO_TESTE_BMP280

// Teste isolado do BMP280: temperatura e pressao a cada 2 s. Soprar no
// sensor sobe a temperatura; a pressao deve ficar estavel (~940 hPa em
// Brasilia, pela altitude).
void app_main(void) {
  i2c_master_bus_handle_t bus = iniciar_barramento_i2c();
  ESP_ERROR_CHECK(sensor_bmp280_init(bus));
  while (true) {
    float temperatura = 0, pressao_hpa = 0;
    esp_err_t err = sensor_bmp280_ler(&temperatura, &pressao_hpa);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha ao ler BMP280: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "temperatura: %.2f C | pressao: %.1f hPa", temperatura,
               pressao_hpa);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_BMP280

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

#if CONFIG_ESTACAO_TESTE_VENTO

#include "sensor_vento.h"

// Teste isolado do reed-switch: janela de 5 s; passar o ima conta
// pulsos (com debounce de 10 ms na ISR).
void app_main(void) {
  ESP_ERROR_CHECK(sensor_vento_init());
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint32_t pulsos = sensor_vento_coletar_pulsos();
    // pulsos em 5 s -> equivalente por minuto (x12)
    ESP_LOGI(TAG, "vento: %lu pulsos na janela de 5 s (%lu/min)",
             (unsigned long)pulsos, (unsigned long)(pulsos * 12));
  }
}

#endif // CONFIG_ESTACAO_TESTE_VENTO

#if CONFIG_ESTACAO_TESTE_LED

#include "led_status.h"

// Teste isolado do LED RGB: cicla as cores de status a cada 2 s e
// depois um fade continuo no canal vermelho (ver o duty trabalhando).
void app_main(void) {
  ESP_ERROR_CHECK(led_status_init());
  const led_status_t ciclo[] = {LED_STATUS_BOOT, LED_STATUS_OK,
                                LED_STATUS_ERRO_SENSOR, LED_STATUS_FALHA,
                                LED_STATUS_APAGADO};
  const char *nomes[] = {"BOOT (azul)", "OK (verde)", "ERRO_SENSOR (amarelo)",
                         "FALHA (vermelho)", "APAGADO"};
  while (true) {
    for (int i = 0; i < 5; i++) {
      ESP_LOGI(TAG, "led: %s", nomes[i]);
      led_status_definir(ciclo[i]);
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI(TAG, "led: fade do vermelho (duty 0->255->0)");
    for (int d = 0; d <= 510; d += 5) {
      led_status_cor(d <= 255 ? d : 510 - d, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

#endif // CONFIG_ESTACAO_TESTE_LED

#if CONFIG_ESTACAO_TESTE_LORA

#include "lora.h"

// Bring-up do SPI do Ra-02: le RegVersion (0x42) a cada 2 s. 0x12 =
// fiacao ok. 0x00/0xFF = radio mudo — conferir 3V3, GND, MISO/MOSI,
// NSS e RST. Nao transmite nada (antena ja deve estar conectada mesmo
// assim).
void app_main(void) {
  ESP_ERROR_CHECK(lora_init());
  while (true) {
    uint8_t versao = 0;
    esp_err_t err = lora_ler_reg(LORA_REG_VERSION, &versao);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "falha na transacao SPI: %s", esp_err_to_name(err));
    } else if (versao == 0x12) {
      ESP_LOGI(TAG, "RegVersion=0x%02X — SX1278 respondendo, fiacao OK", versao);
    } else {
      ESP_LOGW(TAG, "RegVersion=0x%02X (esperado 0x12) — radio mudo, conferir fiacao", versao);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_LORA

#if CONFIG_ESTACAO_TESTE_LORA_TX

#include <stdio.h>

#include "lora.h"

// Primeiro TX de verdade: um pacote curto a cada 5 s em SF12 (simbolo
// de 32,8 ms — chirps visiveis no waterfall do SDR) e potencia minima
// (+2 dBm). Validacao: no gqrx em 433,0 MHz, um claro de ~1 s deve
// aparecer no instante exato de cada log de TxDone.
void app_main(void) {
  ESP_ERROR_CHECK(lora_init());

  uint8_t versao = 0;
  ESP_ERROR_CHECK(lora_ler_reg(LORA_REG_VERSION, &versao));
  if (versao != 0x12) {
    ESP_LOGE(TAG, "RegVersion=0x%02X — radio mudo, conferir fiacao antes do TX",
             versao);
    return;
  }
  ESP_ERROR_CHECK(lora_config_modem());

  uint32_t n = 0;
  while (true) {
    char msg[24];
    int len = snprintf(msg, sizeof(msg), "chirp #%lu", (unsigned long)n);
    ESP_LOGI(TAG, "TX #%lu (%d bytes)...", (unsigned long)n, len);

    uint32_t ms = 0;
    esp_err_t err = lora_tx((const uint8_t *)msg, (uint8_t)len, &ms);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "TxDone em %lu ms (esperado ~1 s em SF12)",
               (unsigned long)ms);
    } else {
      ESP_LOGE(TAG, "TX falhou: %s", esp_err_to_name(err));
    }

    n++;
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

#endif // CONFIG_ESTACAO_TESTE_LORA_TX
