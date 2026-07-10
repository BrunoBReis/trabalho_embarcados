#pragma once

#include "esp_err.h"
#include "stdint.h"

// Configura o reed-switch (GPIO 25): pull-up interno + interrupcao na
// borda de descida + debounce de 10 ms na ISR.
esp_err_t sensor_vento_init(void);

// Devolve os pulsos acumulados desde a ultima chamada (e zera).
// A conversao pulsos -> velocidade (fator do anemometro) e da Fase 7.
uint32_t sensor_vento_coletar_pulsos(void);
