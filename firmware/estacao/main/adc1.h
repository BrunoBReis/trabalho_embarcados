#pragma once

#include "esp_adc/adc_oneshot.h"

// O adc_oneshot permite criar a unidade ADC1 UMA vez por firmware.
// Este modulo e o dono do recurso: cria na primeira chamada e entrega
// o mesmo handle a todos os sensores analogicos (LDR, chuva).
adc_oneshot_unit_handle_t adc1_obter(void);
