# PLANO.md — Roteiro de execução por fases

Cada fase tem **objetivo**, **passos**, **critério de aceitação** (o que
precisa estar funcionando para avançar) e **o que devo aprender** (usar
como pauta das explicações). Executar em ordem. Ao concluir uma fase:
documentar em `docs/`, commitar e marcar a caixa.

---

## Fase 0 — Ambiente e hello world

**Objetivo:** ciclo build → flash → monitor funcionando via Docker.

- [ ] Adicionar usuário ao grupo `uucp` e validar acesso a `/dev/ttyACM0`
      (placa conectada, `dmesg` mostrando o CP2102/CH340)
- [x] Colocar o `Makefile` e o `.clangd` na raiz do repositório; validar
      que `make` (help) e `make shell` funcionam com a imagem
      `espressif/idf:release-v5.3`
- [x] Criar o projeto em `firmware/estacao` (via `make shell` +
      `idf.py create-project`) e rodar `make set-target`
- [x] Hello world em `app_main()`: log a cada 1 s com contador, usando
      `ESP_LOGI` + `vTaskDelay`; compilar com `make build`
- [ ] `make lsp-setup` (uma vez, ~3 GB) e clangd funcionando no LazyVim
      (autocomplete e go-to-definition das APIs do IDF; o
      `compile_commands.json` é encontrado automaticamente no `build/`)
- [ ] Repositório git iniciado; primeiro commit; `docs/00-ambiente.md`

**Aceitação:** vejo o contador subindo no `make run`, e o
LazyVim navega para definições do ESP-IDF.

**Aprender:** o que o build do IDF faz (CMake, bootloader, tabela de
partições, app); o que é `app_main` vs `main`; o que o FreeRTOS está
fazendo por baixo; por que fixar a versão da imagem Docker; anatomia do
Makefile (alvos, variáveis sobrescrevíveis, `.PHONY`, alvo padrão).

---

## Fase 1 — Primeiro sensor: BMP280 (I²C)

**Objetivo:** entender I²C lendo temperatura e pressão reais.

- [ ] Explicação: como funciona o barramento I²C (SDA/SCL, endereços,
      pull-ups) e o driver `i2c_master` do IDF v5
- [ ] Scanner I²C: varrer o barramento e imprimir o endereço encontrado
      (esperado 0x76 ou 0x77) — primeiro código "de verdade"
- [ ] Ler o registrador de ID do chip (0xD0 deve retornar 0x58) na mão,
      para entender leitura de registradores
- [ ] Integrar leitura completa (calibração + compensação) — pode usar
      componente pronto, mas explicando o que ele faz
- [ ] Log periódico: temperatura (°C) e pressão (hPa) a cada 5 s
- [ ] `docs/01-bmp280.md`

**Aceitação:** valores plausíveis para Brasília (~26 °C, ~890 hPa pela
altitude de ~1100 m) no monitor serial.

**Aprender:** registradores, datasheet, diferença entre valor bruto e
compensado, por que o BMP280 precisa de coeficientes de calibração.

---

## Fase 2 — Demais sensores, um por vez

**Objetivo:** dominar um periférico novo da ESP32 a cada sensor.

Ordem (cada item = um mini-projeto de teste isolado + depois integração):

- [ ] **LDR no ADC** (GPIO 34): configurar ADC1 com atenuação de 12 dB,
      ler 0–4095, entender ruído e média de amostras
- [ ] **MH-RD** (GPIO 35 AO, 27 DO): mesma base do ADC; comparar AO vs DO
      molhando a placa
- [ ] **DHT11** (GPIO 4): protocolo single-wire com timing crítico —
      usar componente pronto e estudar o timing no datasheet
- [ ] **Reed-switch** (GPIO 25): interrupção GPIO, pull-up interno,
      debounce por tempo (ignorar pulsos < 10 ms); contar pulsos/minuto
- [ ] **LED RGB** (16/17/13): periférico LEDC (PWM), tabela de cores de
      status (boot, ok, erro de sensor, LoRa tx/falha)
- [ ] `docs/02-...` a `docs/06-...` (um por sensor)

**Aceitação:** cada sensor tem um teste isolado que roda e imprime
valores coerentes; consigo explicar cada linha do código.

**Aprender:** ADC (resolução, atenuação, ruído), interrupções vs
polling, debounce, PWM/LEDC, por que protocolos com timing crítico
(DHT11) são chatos num RTOS.

---

## Fase 3 — Firmware integrado da estação

**Objetivo:** todos os sensores juntos, dados no formato final.

- [ ] Montagem completa na protoboard conferindo o mapa de pinos do
      CLAUDE.md
- [ ] Definir em `common/` o pacote binário (struct packed com ponto
      fixo: seq, temp_x100, press_pa, umid, luz_raw, chuva_raw,
      vento_rpm, flags, crc16) — ~16 bytes
- [ ] Tarefa principal: a cada 30 s, ler todos os sensores, preencher a
      struct, imprimir legível E em hex no monitor
- [ ] Flags de erro por sensor (falha de leitura não derruba o sistema)
- [ ] LED RGB refletindo o estado real
- [ ] `docs/07-integracao.md` com foto da protoboard

**Aceitação:** monitor mostra o pacote completo, sensores falhos são
marcados nas flags sem travar o loop. **Este é o marco "dados no formato
que eu quero" — só depois dele entra o LoRa.**

**Aprender:** organização de firmware em módulos, ponto fixo vs float,
CRC, tolerância a falhas, por que payload binário em vez de JSON no rádio.

---

## Fase 4 — Enlace LoRa (Ra-02 SX1278)

**Objetivo:** os dois nós conversando por rádio.

- [ ] ⚠️ Conferir antenas conectadas nos DOIS módulos antes de qualquer TX
- [ ] Criar o projeto do segundo nó em `firmware/gateway` (compilar/gravar
      com `make ... PROJ=firmware/gateway PORT=/dev/ttyUSB1`)
- [ ] Explicação: CSS/chirp, SF, BW, CR e o trade-off alcance × taxa ×
      tempo no ar; por que 433 MHz na bancada vs AU915 num produto real
- [ ] Integrar o componente `esp-idf-sx127x` (nopnop2002) ou driver SPI
      próprio; entender NSS/RST/DIO0
- [ ] Ping-pong: nó A manda "oi", nó B responde "recebi" — parâmetros
      idênticos nos dois lados (freq 433 MHz, SF7, BW 125 kHz para começar)
- [ ] Substituir o payload pelo pacote da Fase 3; validar CRC e seq no
      receptor; medir RSSI/SNR
- [ ] Teste de perda: afastar os nós, observar seq pulando
- [ ] `docs/08-lora.md`

**Aceitação:** gateway recebe o pacote real da estação, valida CRC,
imprime os campos decodificados + RSSI, e detecta pacotes perdidos.

**Aprender:** SPI, interrupção externa (DIO0), parâmetros LoRa na
prática, o que se perde sem LoRaWAN (ACK/endereçamento) e como o
seq+CRC mitiga.

---

## Fase 5 — Gateway: Wi-Fi + MQTT (aqui começa o DevOps de verdade)

**Objetivo:** dados do rádio chegando num broker MQTT.

- [ ] `infra/docker-compose.yml` com Mosquitto (config mínima com
      listener 1883 e log)
- [ ] Validar broker com `mosquitto_pub`/`mosquitto_sub` locais antes de
      envolver a ESP32
- [ ] No gateway: Wi-Fi STA (componente `esp_wifi` + tratamento de
      reconexão) — entender o event loop do IDF
- [ ] Componente `esp-mqtt`: publicar cada pacote recebido; definir
      esquema de tópicos (`estacao/v1/temperatura`, ... ou JSON único em
      `estacao/v1/dados` — decidir e documentar)
- [ ] QoS: começar com 0, entender quando 1 faria sentido
- [ ] `docs/09-mqtt.md`

**Aceitação:** `mosquitto_sub -t 'estacao/#' -v` no PC mostra os dados
da estação chegando pelo caminho completo sensor → LoRa → Wi-Fi → broker.

**Aprender:** MQTT (tópicos, QoS, retain), event loop e ciclo de vida do
Wi-Fi no IDF, docker-compose, volumes e configuração de serviços.

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
