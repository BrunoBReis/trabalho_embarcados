# 05 — Reed-switch, contagem de vento (Fase 2)

## O que foi feito

- Módulo `sensor_vento.c/.h` (KY-021 no GPIO 25): interrupção na borda de
  descida, pull-up interno (em paralelo com o 10 kΩ da plaquinha),
  **debounce de 10 ms dentro da ISR**, contador atômico com leitura-e-zera
  (`atomic_exchange`).
- Teste isolado no menuconfig: janela de 5 s, log de pulsos e equivalente
  por minuto.

## Conceitos

- **Interrupção vs polling**: polling gastaria CPU olhando o pino 1000x/s;
  a interrupção deixa o hardware vigiar — a ISR só roda quando o evento
  acontece.
- **Regras de ISR**: curta (incrementa e sai), sem log/bloqueio, so APIs
  seguras em interrupcao (`esp_timer_get_time` pode; `ESP_LOGx` nao),
  codigo em RAM (`IRAM_ATTR`) para nao depender da flash.
- **Debounce**: contato mecânico quica (rajada de micro-pulsos em ~1 ms).
  Filtro por tempo na ISR: ignora pulso a < 10 ms do anterior — rápido
  demais para ser volta de ímã, lento o bastante para engolir o bounce.
- **Atomicidade**: contador escrito pela ISR e lido pela tarefa;
  `atomic_exchange(&pulsos, 0)` lê e zera sem janela para perder pulso.
- **O reed sente ímã, não vento**: o anemômetro real é mecânico — copos
  giram, ímã no eixo passa pelo reed 1x por volta. Pulsos/min → velocidade
  é o fator de calibração da Fase 7.

## Validação na bancada

1. **Jumper GND→S** (simula o reed fechando): contagens batem com os
   toques — prova ISR + debounce + contador. O dedo humano é fonte severa
   de bounce; contagens próximas do nº de toques = debounce trabalhando.
2. **Ímã no tubinho de vidro**: pulso por passada (passada lenta pode
   contar 2 — o ímã entra e sai do alcance; considerar no fator da Fase 7).

