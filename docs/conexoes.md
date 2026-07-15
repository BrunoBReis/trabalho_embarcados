# Conexões do hardware

A definição dos componenetes foi feita antes do desenvolvimento do firmaware,
basta ver em [Pinagem](./pinagem.md).
Esta página apresenta a topologia — o que liga em quê, por qual barramento.

## ESP32

![ESP32 Pinout](./assets/esp32_pinout.png)

## Diagrama de fiação (WireViz)

Gerado a partir de [`docs/wireviz/estacao.yml`](https://github.com/BrunoBReis/trabalho_embarcados/blob/main/docs/wireviz/estacao.yml)
com o [WireViz](https://github.com/wireviz/WireViz) — mexeu na fiação.

![Diagrama de fiação da estação](wireviz/estacao.svg)

!!! note "Por que duas visões?"
    O WireViz mostra o *chicote físico* (fio a fio, com cores e
    numeração de pinos); o Mermaid abaixo mostra a *topologia lógica*
    (quem fala com quem, por qual barramento). A tabela mais abaixo
    continua sendo o contrato canônico.

## Topologia por barramento

```mermaid
flowchart LR
    ESP["ESP32 WROOM-32<br/>(30 pinos)"]

    subgraph i2c["I²C (400 kHz)"]
        BMP["BMP280<br/>0x76"]
    end
    subgraph spi["SPI (VSPI @ 1 MHz)"]
        RA["Ra-02 SX1278<br/>+ antena via pigtail/BPF"]
    end
    subgraph analogicos["ADC1 (12 bits, 12 dB)"]
        LDR["LDR KY-018"]
        MHRD_A["MH-RD (AO)"]
    end
    subgraph digitais["GPIO"]
        DHT["DHT11"]
        MHRD_D["MH-RD (DO)"]
        REED["reed switch"]
        LED["LED RGB (LEDC/PWM)"]
    end

    BMP ---|"SDA→21 · SCL→22"| ESP
    RA ---|"SCK→18 · MISO→19 · MOSI→23<br/>NSS→5 · RST→14 · DIO0→26"| ESP
    LDR ---|"AO→34"| ESP
    MHRD_A ---|"AO→35"| ESP
    DHT ---|"dados→4"| ESP
    MHRD_D ---|"DO→27"| ESP
    REED ---|"25 (pull-up interno + debounce)"| ESP
    ESP ---|"R→16 · G→17 · B→13"| LED
```

## Tabela canônica

| Função | GPIO | Nota |
|---|---|---|
| I²C SDA (BMP280) | 21 | |
| I²C SCL (BMP280) | 22 | |
| DHT11 dados | 4 | módulo de 3 pinos (pull-up na plaquinha) |
| LDR AO | 34 | ADC1_CH6, somente entrada |
| MH-RD AO | 35 | ADC1_CH7, somente entrada |
| MH-RD DO | 27 | opcional (limiar do trimpot) |
| Reed switch | 25 | interrupção + pull-up interno + debounce 10 ms |
| LoRa SCK | 18 | VSPI |
| LoRa MISO | 19 | VSPI (pull-up de diagnóstico no driver) |
| LoRa MOSI | 23 | VSPI |
| LoRa NSS (CS) | 5 | conduzido pelo driver SPI |
| LoRa RST | 14 | reset por hardware no boot |
| LoRa DIO0 | 26 | IRQ TxDone/RxDone (reservado; TX atual usa polling) |
| LED R / G / B | 16 / 17 / 13 | PWM via LEDC; módulo anodo comum |

**Alimentação:** tudo em **3V3**; Ra-02 com dois
GNDs ligados.

## Cadeia de RF

```mermaid
flowchart TD
    SX["Ra-02<br/>PA_BOOST +2 dBm"] --- IPEX["IPEX/u.FL"]
    IPEX --- PIG["pigtail<br/>(SMA macho)"]
    PIG --- BPF["BPF 433 MHz<br/>(SMA fêmea ×2 — faz de emenda)"]
    BPF --- ANT1["antena 433 MHz"]
    ANT2["antena 433 MHz"] --- RTLSDR["RTL-SDR v3<br/>(SMA fêmea)"]
    ANT1 -. "433,0 MHz" .- ANT2
```


## Mapa visual da placa

![Mapa de pinagem do ESP32 na estação](assets/mapa_pinagem_esp32_estacao.svg)
