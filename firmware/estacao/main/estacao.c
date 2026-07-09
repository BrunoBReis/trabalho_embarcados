#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "estacao";

void app_main(void)
{
    uint32_t contador = 0;

    ESP_LOGI(TAG, "Estacao meteorologica — hello world (Fase 0)");

    while (true) {
        ESP_LOGI(TAG, "contador: %lu", contador);
        contador++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
