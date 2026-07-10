LoRa é lento. Absurdamente lento — de propósito.

A taxa depende dos três parâmetros que você vai configurar na Fase 4 (SF, BW, CR). Com BW 125 kHz e CR 4/5 (nosso ponto de partida):

┌───────────────────────┬───────────┬───────────────────────────────┐
│   Spreading Factor    │ Taxa útil │ Nosso pacote de 16 bytes leva │
├───────────────────────┼───────────┼───────────────────────────────┤
│ SF7 (curto alcance)   │ ~5,5 kbps │ ~35 ms no ar                  │
├───────────────────────┼───────────┼───────────────────────────────┤
│ SF9                   │ ~1,8 kbps │ ~165 ms                       │
├───────────────────────┼───────────┼───────────────────────────────┤
│ SF12 (máximo alcance) │ ~290 bps  │ ~1,2 s                        │
└───────────────────────┴───────────┴───────────────────────────────┘

Para calibrar a intuição: seu Wi-Fi faz ~100 Mbps — o LoRa em SF12 é ~300.000 vezes mais lento. É uma troca deliberada: cada bit é "espalhado" em chirps longos que o receptor consegue pescar abaixo do ruído de fundo, e é isso que compra quilômetros de alcance com miliwatts. Alcance × taxa é o cabo de guerra fundamental do rádio.

E há mais dois tetos além da taxa:

- Payload máximo por pacote: 255 bytes (limite do FIFO do SX1278) — e na prática bem menos em SF alto.
- Etiqueta de espectro: as bandas ISM têm regras de ocupação (na Europa, 1% de duty cycle — você só pode ocupar o ar 36 s por hora!). Não é só cortesia: tempo no ar é energia da bateria e colisão com vizinhos.

Por que isso mata o JSON no rádio

Se mandássemos {"temp":26.43,"press":889.2,"umid":45,...} seriam ~120–200 bytes — em SF12, ~10 s no ar por leitura. Inviável. O pacote binário da Fase 3 espreme as mesmas informações em ~16 bytes:

- temp_x100 como int16 (2 bytes representam −327,68…+327,67 °C com 0,01 °C de passo — ponto fixo: o truque de guardar 26,43 como o inteiro 2643);
- press_pa em uint32, umid em 1 byte, e assim por diante;
  - seq (detectar perda) e crc16 (detectar corrupção) — os 4 bytes de "seguro" que substituem o que o LoRaWAN faria por nós.

O JSON volta a existir depois — no gateway, que fala MQTT por Wi-Fi, onde bytes são grátis. Formato certo para cada enlace: binário espremido no rádio caro, JSON legível na rede barata.
