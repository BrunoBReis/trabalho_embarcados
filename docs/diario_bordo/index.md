# Diário de bordo

A história real do projeto, etapa por etapa: o que foi feito, os
conceitos aprendidos, os problemas encontrados e como cada passo foi
validado no monitor serial. É a parte mais valiosa para quem quiser
reproduzir o trabalho — os defeitos de hardware e as mudanças de rota
estão registrados junto com as soluções.

## Fundação

- **[00 — Ambiente](00-ambiente.md)** — ciclo build → flash → monitor
  100% dockerizado, Makefile orquestrando o `idf.py` e clangd
  funcionando no LazyVim.
- **[01 — BMP280 (I²C)](01-bmp280.md)** — scanner I²C, leitura de
  registradores na mão, driver via Component Manager e o diagnóstico do
  módulo defeituoso.

## Um sensor por vez

- **[02 — LDR (ADC)](02-ldr.md)** — primeiro contato com o ADC1
  (oneshot, atenuação, média de 16 amostras) e o LDR soldado nos furos
  errados de fábrica.
- **[03 — Chuva (MH-RD)](03-chuva.md)** — saída dupla AO + DO e a
  refatoração do ADC1 para o padrão singleton.
- **[04 — DHT11](04-dht11.md)** — a aula do timing crítico: protocolo
  de 1 fio onde cada bit é a duração de um pulso.
- **[05 — Vento (reed)](05-vento.md)** — interrupção, debounce de 10 ms
  na ISR e contador atômico.
- **[06 — LED RGB (LEDC)](06-led-rgb.md)** — PWM por hardware e a
  tabela de cores de status da estação.

## Integração e enlace

- **[07 — Integração](07-integracao.md)** — todos os sensores num ciclo
  só, o pacote de 18 bytes com CRC16 em `common/pacote` e a tolerância
  a falhas por flags.
- **[08 — LoRa + SDR](08-lora.md)** — a maior decisão de arquitetura:
  sem segundo Ra-02, o receptor vira RTL-SDR + GNU Radio; bring-up do
  SPI, chirps no waterfall e as batalhas de RF.

## Gateway e visualização

- **[09 — MQTT](09-mqtt.md)** — broker Mosquitto no compose e a
  PonteMqtt validando CRC e publicando JSON.
- **[10 — Dashboard](10-dashboard.md)** — o browser como cliente MQTT
  via WebSocket: dashboard sem backend.
