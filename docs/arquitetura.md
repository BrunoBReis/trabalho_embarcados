# Arquitetura

## Visão geral

Dois "mundos" ligados por rádio: o **nó externo** (ESP32, sem nenhuma
infraestrutura além de alimentação) e o **gateway** (o PC, onde tudo é
software em containers). A decisão de arquitetura mais importante do
projeto está no meio: sem um segundo módulo LoRa, o receptor é um
RTL-SDR com o PHY LoRa implementado em GNU Radio (`docs/08-lora.md`
conta essa história).

```mermaid
flowchart TB
    subgraph no_externo["Nó externo — firmware/estacao (ESP-IDF v5.3, FreeRTOS)"]
        direction LR
        BMP["BMP280<br/>temp + pressão"] -->|I²C| APP["tarefa principal<br/>(1 ciclo / 10 s)"]
        DHT["DHT11<br/>umidade"] -->|single-wire| APP
        LDR["LDR<br/>luminosidade"] -->|ADC1| APP
        MHRD["MH-RD<br/>chuva"] -->|ADC1 + GPIO| APP
        REED["reed switch<br/>vento"] -->|IRQ + debounce| APP
        APP --> PAC["common/pacote<br/>18 B + CRC16"]
        PAC --> LORA["main/lora.c<br/>driver SPI próprio"]
        APP --> LEDRGB["LED RGB<br/>verde/amarelo/vermelho"]
    end

    LORA -. "chirps CSS<br/>433,0 MHz" .-> RTL

    subgraph gateway["Gateway — infra/ (Docker Compose)"]
        direction LR
        RTL["RTL-SDR v3<br/>250 kS/s"] --> CHAIN["gr-lora_sdr<br/>frame_sync → FFT →<br/>Hamming → CRC"]
        CHAIN -->|porta msg| PMQTT["PonteMqtt<br/>valida CRC16<br/>desserializa → JSON"]
        PMQTT -->|"publish estacao/v1/dados<br/>QoS 0 + retain"| BROKER[("Mosquitto 2.0<br/>:1883 · ws :9001")]
        BROKER -->|WebSocket| WEB["dashboard (nginx)<br/>browser = cliente MQTT"]
    end
```

## A vida de um pacote

```mermaid
sequenceDiagram
    participant S as Sensores
    participant E as ESP32
    participant R as SX1278
    participant G as RTL-SDR + gr-lora_sdr
    participant P as PonteMqtt
    participant B as Mosquitto
    participant W as Browser

    loop a cada 10 s
        E->>S: lê I²C, single-wire, ADC, pulsos
        E->>E: monta pacote (18 B) + CRC16-CCITT
        E->>R: SPI: payload na FIFO, modo TX
        R--)G: ~1,3 s de chirps no ar (SF12)
        G->>G: sincroniza, demodula, decodifica PHY
        G->>P: payload cru (porta de mensagens PMT)
        P->>P: CRC16 confere? desserializa : descarta
        P->>B: publish JSON (retain)
        B--)W: push via WebSocket
        Note over W: cards atualizam,<br/>flags de sensor em vermelho
    end
```

## Tolerância a falhas (herdada da Fase 3)

- **Sensor falhou** → bit em `flags`, campo zerado, pacote sai mesmo
  assim; o dashboard pinta o card do sensor de vermelho.
- **Rádio mudo no boot** → `[SELFTEST] lora FALHA`, LED vermelho, e a
  estação segue lendo sensores e logando (sem TX).
- **Pacote corrompido no ar** → o CRC16 fim-a-fim não confere na ponte;
  vira contador de descarte, nunca dado falso no broker.
- **Broker caiu** → paho reconecta sozinho; o `retain` repõe o último
  estado para qualquer assinante novo.

## Dois CRCs, de propósito

| CRC | Quem calcula | O que protege |
|---|---|---|
| CRC do quadro LoRa | SX1278 (TX) / gr-lora_sdr (RX) | só o trecho aéreo |
| CRC16-CCITT do pacote | firmware (TX) / PonteMqtt (RX) | fim-a-fim: rádio, decodificador, broker, banco futuro |

## Decisões de arquitetura registradas

1. **Gateway em software (SDR)** — sem segundo Ra-02; o PHY LoRa roda
   em GNU Radio dentro de container ([08 — LoRa](08-lora.md)).
2. **JSON único em `estacao/v1/dados`** — Telegraf/Grafana futuros
   consomem sem mudar nada ([09 — MQTT](09-mqtt.md)).
3. **Browser como cliente MQTT** — dashboard sem backend, via listener
   WebSocket do Mosquitto ([10 — Dashboard](10-dashboard.md)).
4. **Reprodutibilidade** — toolchain, imagens e dependências fixadas
   por versão/commit; tudo sobe com `make`.
