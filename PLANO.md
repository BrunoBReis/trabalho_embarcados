# PLANO.md — Roteiro de execução por fases

Cada fase tem **objetivo**, **passos**, **critério de aceitação** (o que
precisa estar funcionando para avançar) e **o que devo aprender** (usar
como pauta das explicações). Executar em ordem. Ao concluir uma fase:
documentar em `docs/`, commitar e marcar a caixa.

---

## Fase 0 — Ambiente e hello world

**Objetivo:** ciclo build → flash → monitor funcionando via Docker.

- [x] Adicionar usuário ao grupo `uucp` e validar acesso a `/dev/ttyACM0`
      (placa conectada, `dmesg` mostrando o CP2102/CH340)
- [x] Colocar o `Makefile` e o `.clangd` na raiz do repositório; validar
      que `make` (help) e `make shell` funcionam com a imagem
      `espressif/idf:release-v5.3`
- [x] Criar o projeto em `firmware/estacao` (via `make shell` +
      `idf.py create-project`) e rodar `make set-target`
- [x] Hello world em `app_main()`: log a cada 1 s com contador, usando
      `ESP_LOGI` + `vTaskDelay`; compilar com `make build`
- [x] `make lsp-setup` (uma vez, ~3 GB) e clangd funcionando no LazyVim
      (autocomplete e go-to-definition das APIs do IDF; o
      `compile_commands.json` é encontrado automaticamente no `build/`)
- [x] Repositório git iniciado; primeiro commit; `docs/00-ambiente.md`

**Aceitação:** vejo o contador subindo no `make run`, e o
LazyVim navega para definições do ESP-IDF.

**Aprender:** o que o build do IDF faz (CMake, bootloader, tabela de
partições, app); o que é `app_main` vs `main`; o que o FreeRTOS está
fazendo por baixo; por que fixar a versão da imagem Docker; anatomia do
Makefile (alvos, variáveis sobrescrevíveis, `.PHONY`, alvo padrão).

---

## Fase 1 — Primeiro sensor: BMP280 (I²C)

**Objetivo:** entender I²C lendo temperatura e pressão reais.

- [x] Explicação: como funciona o barramento I²C (SDA/SCL, endereços,
      pull-ups) e o driver `i2c_master` do IDF v5
- [x] Scanner I²C: varrer o barramento e imprimir o endereço encontrado
      (esperado 0x76 ou 0x77) — primeiro código "de verdade"
- [x] Ler o registrador de ID do chip (0xD0 deve retornar 0x58) na mão,
      para entender leitura de registradores
- [x] Integrar leitura completa (calibração + compensação) — pode usar
      componente pronto, mas explicando o que ele faz
- [x] Log periódico: temperatura (°C) e pressão (hPa) a cada 5 s
- [x] `docs/01-bmp280.md`

**Aceitação:** valores plausíveis para Brasília (~26 °C, ~890 hPa pela
altitude de ~1100 m) no monitor serial.

**Aprender:** registradores, datasheet, diferença entre valor bruto e
compensado, por que o BMP280 precisa de coeficientes de calibração.

**Obs**: o sensor não está com boa qualidade, por conta disso não foi possível pegar os dados reais.

---

## Fase 2 — Demais sensores, um por vez

**Objetivo:** dominar um periférico novo da ESP32 a cada sensor.

Ordem (cada item = um mini-projeto de teste isolado + depois integração):

- [x] **LDR no ADC** (GPIO 34): configurar ADC1 com atenuação de 12 dB,
      ler 0–4095, entender ruído e média de amostras
      **Obs (pendência de hardware)**: módulo KY-018 veio com o LDR
      soldado em furos não conectados (ver docs/02-ldr.md); software
      validado com jumper no 3V3 (leu 4095). Consertar módulo ou montar
      divisor com LDR avulso + resistor ~10 kΩ quando disponível.
- [x] **MH-RD** (GPIO 35 AO, 27 DO): mesma base do ADC; comparar AO vs DO
      molhando a placa
- [x] **DHT11** (GPIO 4): protocolo single-wire com timing crítico —
      usar componente pronto e estudar o timing no datasheet
- [x] **Reed-switch** (GPIO 25): interrupção GPIO, pull-up interno,
      debounce por tempo (ignorar pulsos < 10 ms); contar pulsos/minuto
- [x] **LED RGB** (16/17/13): periférico LEDC (PWM), tabela de cores de
      status (boot, ok, erro de sensor, LoRa tx/falha)
      **Obs**: módulo Wcmcu é anodo comum (silkscreen "-" enganoso, comum
      no 3V3, output_invert no LEDC) e sem resistores — teto de duty 25%.
      Comprar 3x 220–330 Ω junto com o 10 kΩ do LDR.
- [x] `docs/02-...` a `docs/06-...` (um por sensor)
      **Extra**: autoteste [SELFTEST] + `tools/bancada.py` +
      `make test-bancada` (aprovado com os 6 componentes)

**Aceitação:** cada sensor tem um teste isolado que roda e imprime
valores coerentes; consigo explicar cada linha do código.

**Aprender:** ADC (resolução, atenuação, ruído), interrupções vs
polling, debounce, PWM/LEDC, por que protocolos com timing crítico
(DHT11) são chatos num RTOS.

---

## Fase 3 — Firmware integrado da estação

**Objetivo:** todos os sensores juntos, dados no formato final.

- [x] Montagem completa na protoboard conferindo o mapa de pinos do
      CLAUDE.md
- [x] Definir em `common/` o pacote binário (struct packed com ponto
      fixo: seq, temp_x100, press_pa, umid, luz_raw, chuva_raw,
      vento_rpm, flags, crc16) — 18 bytes (componente `common/pacote`)
- [x] Tarefa principal: a cada 30 s, ler todos os sensores, preencher a
      struct, imprimir legível E em hex no monitor
- [x] Flags de erro por sensor (falha de leitura não derruba o sistema)
      — validado ao vivo: DHT11 arrancado/replugado sem reboot
- [x] LED RGB refletindo o estado real
- [x] `docs/07-integracao.md` (**pendência**: anexar foto da protoboard)

**Aceitação:** monitor mostra o pacote completo, sensores falhos são
marcados nas flags sem travar o loop. **Este é o marco "dados no formato
que eu quero" — só depois dele entra o LoRa.**

**Aprender:** organização de firmware em módulos, ponto fixo vs float,
CRC, tolerância a falhas, por que payload binário em vez de JSON no rádio.

---

## Fase 4 — Enlace LoRa (Ra-02 SX1278 → RTL-SDR)

**Objetivo:** a estação transmitindo LoRa de verdade, recebido no PC.

**Replanejada (jul/2026):** só há UM Ra-02; os módulos FS1000A/MX-RM-5V
são ASK/OOK e não falam LoRa. O receptor passa a ser o PC com RTL-SDR v3
+ `gr-lora_sdr` (ver `docs/08-lora.md`). O segundo ESP32 sai do caminho
do rádio. Risco maior do plano: a decodificação via SDR — validar cedo.

- [x] Decisão de arquitetura documentada (`docs/08-lora.md`)
- [x] Explicação: CSS/chirp, SF, BW, CR, tempo no ar (AN1200.22)
- [x] Driver SPI próprio (`main/lora.c`): VSPI @ 1 MHz, reset via RST,
      leitura de registradores; **RegVersion 0x42 → 0x12 validado**
      (pegadinha encontrada: MOSI em furo vazio da protoboard)
- [x] ⚠️ Antena do Ra-02: cadeia `pigtail → BPF → antena` (pino no
      encaixe nas duas junções); o BPF sem serigrafia foi aprovado
      empiricamente — o chirp atravessa forte, logo passa 433 MHz
- [x] Configurar o modem: 433,0 MHz, SF12/BW125 didático, potência
      mínima (+2 dBm na bancada, fixa no código); TX com TxDone por
      polling (DIO0 fica para o RX)
- [x] Validar o chirp no waterfall (gqrx): faixas de 125 kHz em
      433,000 MHz, ~1 s de duração, a cada 5 s — 4 critérios fechados
- [x] `gr-lora_sdr` (Docker) decodificando no PC: "chirp #N" com header
      e CRC válidos, seq contínua (batalhas do debug em docs/08-lora.md:
      GR_PYTHON_DIR no 24.04, buffer do SF12 e a forma de 2 argumentos
      do set_min_output_buffer que o issue #55 nunca descobriu)
- [x] Payload = pacote de 18 B da Fase 3 transmitido pelo modo estação
      (intervalo 10 s p/ bancada) e recebido íntegro no PC: seq
      contínua, flags 0x00, campos decodificados batendo com os
      sensores (validação do CRC16 no PC fica para a ponte MQTT da
      Fase 5, onde o pacote é desserializado)
- [ ] *(adiado — extra pós-Fase 6)* Voltar para SF7/BW125 (enlace de
      produção); promover o SF a opção do menuconfig
- [ ] *(adiado — extra pós-Fase 6)* Teste de perda: afastar estação do
      SDR, observar seq pulando (decisão de 12/07/2026: MQTT e
      dashboard primeiro — pipeline completo vale mais que enlace
      otimizado)
- [ ] `docs/08-lora.md` completo

**Aceitação:** PC recebe o pacote real da estação via SDR, valida CRC,
imprime os campos decodificados e detecta pacotes perdidos.

**Aprender:** SPI, interrupção externa (DIO0), parâmetros LoRa na
prática, SDR/waterfall, o que se perde sem LoRaWAN (ACK/endereçamento)
e como o seq+CRC mitiga.

**Plano B** (se a decodificação SDR emperrar): segundo Ra-02 no
`firmware/gateway`, voltando ao desenho original.

---

## Fase 5 — Gateway no PC: decodificador + MQTT (aqui começa o DevOps de verdade)

**Objetivo:** dados do rádio chegando num broker MQTT.

**Replanejada junto com a Fase 4:** o gateway é o PC. A ponte
rádio → broker deixa de ser `esp_wifi`/`esp-mqtt` num ESP32 e vira um
serviço no docker-compose (decodificador SDR → publicador MQTT).

- [x] `infra/docker-compose.yml` com Mosquitto 2.0.20 (listener 1883,
      anônimo de bancada, persistência em volume nomeado, log stdout)
- [x] Validar broker com `mosquitto_pub`/`mosquitto_sub` (round-trip
      pub/sub em `estacao/#` dentro do container)
- [x] Ponte SDR → MQTT: bloco `PonteMqtt` no receptor (porta de
      mensagens do crc_verif → CRC16 fim-a-fim → JSON via paho-mqtt);
      tópico único `estacao/v1/dados`; validada com pacotes reais da
      Fase 4 reinjetados via PMT e depois AO VIVO na bancada (12/07)
- [x] QoS 0 + retain (telemetria tolera perda; QoS 1 seria p/ comandos)
      — decisão documentada em docs/09-mqtt.md
- [x] `docs/09-mqtt.md`

**Aceitação:** ✅ (12/07/2026) `mosquitto_sub -t 'estacao/#' -v` no PC
mostra os dados da estação chegando pelo caminho completo
sensor → LoRa → SDR → broker.

**Aprender:** MQTT (tópicos, QoS, retain), docker-compose, volumes,
serviços conversando entre containers (decoder → broker).

**Nota:** o aprendizado embarcado que sai de cena (`esp_wifi`,
`esp-mqtt`, event loop do IDF) pode voltar como extra da Fase 7 num modo
diagnóstico da estação, se sobrar tempo.

---

## Fase 6 — Dashboard e persistência

**Objetivo:** visualização histórica; fechar o pipeline.

- [ ] Ampliar o docker-compose: Mosquitto + InfluxDB + Telegraf (consumer
      MQTT) + Grafana — ou Node-RED como alternativa mais simples;
      discutir o trade-off e escolher
- [ ] Dashboard com séries de temperatura, umidade, pressão, luz, chuva
      e vento
- [ ] Retenção de dados e restart automático dos containers
- [ ] `docs/10-dashboard.md`

**Aceitação:** derrubar e subir a infra com um comando
(`docker compose up -d`) e ver o histórico sobrevivendo a restart.

**Aprender:** séries temporais, pipeline pub/sub → banco → visualização,
redes entre containers, volumes persistentes.

---

## Fase 7 — Qualidade, calibração e fechamento

**Objetivo:** transformar o protótipo em resultado apresentável.

- [ ] Calibração: curva do LDR ao longo de um dia, limiares da chuva,
      fator pulsos→velocidade do vento (documentar as limitações)
- [ ] Teste de alcance LoRa fora da bancada (distâncias e RSSI numa tabela)
- [ ] CI no GitHub Actions: job que builda os dois firmwares com a mesma
      imagem Docker do desenvolvimento (DevOps: build reprodutível)
- [ ] Revisão final da documentação em `docs/` + atualização do README
      da disciplina com resultados

**Aceitação:** clone limpo do repositório compila via CI, e o `docs/`
conta a história completa do projeto.

---

## Como pretendo usar o Claude Code neste plano

- Trabalhar **uma fase por sessão** (ou menos); começar cada sessão
  pedindo para ler CLAUDE.md e PLANO.md e propor o plano do passo atual
  antes de codar (usar o plan mode).
- Pedir explicações antes do código e quiz rápido ao final de cada fase
  para verificar meu entendimento.
- Nunca aceitar código que eu não consiga explicar.
