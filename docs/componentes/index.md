# Componentes

Cada periférico da estação tem uma página no mesmo formato: **foto**,
**o que é**, **conexão com o ESP32** (tabela de pinos), **tipo de
comunicação** e um tópico extra que valeu registro — com link para o
diário de bordo onde a história completa está contada.

## Sensores

- **[BMP280 — temperatura/pressão](BMP280.md)** — sensor digital da
  Bosch com calibração de fábrica; fala **I²C** no endereço 0x76.
- **[DHT11 — umidade](DHT11.md)** — protocolo proprietário de **1 fio**
  em que cada bit é a duração de um pulso; exige seção crítica no
  driver.
- **[LDR KY-018 — luminosidade](LDR.md)** — divisor de tensão lido pelo
  **ADC1**; veio de fábrica com o LDR soldado nos furos errados.
- **[MH-RD + LM393 — chuva](pente_LM393.md)** — pente resistivo com
  saída dupla: **analógica** (intensidade) e **digital** (limiar no
  trimpot).
- **[Reed switch — vento](reed.md)** — interruptor magnético que gera
  **pulsos por interrupção**; debounce de 10 ms na ISR.

## Atuador e rádio

- **[LED RGB — status](LED.md)** — 3 canais **PWM (LEDC)** indicando
  verde/amarelo/vermelho; anodo comum resolvido com `output_invert`.
- **[LoRa Ra-02 — rádio](LoRa.md)** — SX1278 a 433 MHz: **SPI** com o
  ESP32, chirps LoRa pelo ar; nunca transmitir sem antena.
