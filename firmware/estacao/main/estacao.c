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
