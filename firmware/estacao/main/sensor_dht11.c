#include "sensor_dht11.h"

#include "dht.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define DHT11_PIN GPIO_NUM_4

static const char *TAG = "sensor_dht11";

esp_err_t sensor_dht11_init(void) {
  // O protocolo e single-wire com a linha em repouso em nivel alto;
  // modulos de 3 pinos ja trazem o pull-up na plaquinha, e o driver
  // conduz o pino sozinho a cada leitura. Nada a configurar aqui alem
  // de deixar o pino livre de outras funcoes.
  gpio_reset_pin(DHT11_PIN);
  ESP_LOGI(TAG, "dados no GPIO %d", DHT11_PIN);
  return ESP_OK;
}

esp_err_t sensor_dht11_ler(float *umidade_pct, float *temperatura_c) {
  // O componente desliga interrupcoes (~5 ms) e cronometra os pulsos
  // de 26/70 us por busy-wait — exigencia do protocolo do DHT11.
  return dht_read_float_data(DHT_TYPE_DHT11, DHT11_PIN, umidade_pct,
                             temperatura_c);
}
