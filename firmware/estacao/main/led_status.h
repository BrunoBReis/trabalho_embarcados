#pragma once

#include "esp_err.h"

// Estados do sistema mapeados em cores (tabela em led_status.c).
typedef enum {
  LED_STATUS_BOOT,        // azul: inicializando
  LED_STATUS_OK,          // verde: tudo funcionando
  LED_STATUS_ERRO_SENSOR, // amarelo: algum sensor falhou (sistema segue)
  LED_STATUS_FALHA,       // vermelho: falha grave
  LED_STATUS_APAGADO,
} led_status_t;

// Configura o LEDC (timer 5 kHz / 8 bits + 3 canais em R=16 G=17 B=13).
esp_err_t led_status_init(void);

// Aplica a cor do estado.
void led_status_definir(led_status_t status);

// Cor arbitraria (0-255 por canal) — util para testes.
void led_status_cor(uint8_t r, uint8_t g, uint8_t b);
