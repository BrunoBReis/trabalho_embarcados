# CLAUDE.md — Estação Meteorológica IoT (ESP32 + LoRa)

## O que é este projeto

Reprodução didática da arquitetura da Davis Vantage Pro2 para a disciplina de
Fundamentos de Sistemas Embarcados (FGA/UnB). Dois nós ESP32: uma **estação**
(nó externo) que lê sensores e transmite por LoRa, e um **gateway** (nó base)
que recebe por LoRa e publica via MQTT para um dashboard. O documento de
referência da arquitetura é o `README.md` do repositório.

## Contexto do desenvolvedor (importante!)

- Esta é a **primeira experiência do autor com sistemas embarcados**. O
  objetivo é APRENDER, não só entregar. Sempre explique O QUE está sendo
  feito e POR QUÊ antes de escrever código.
- Segundo objetivo de aprendizado: **DevOps** (Docker, docker-compose,
  reprodutibilidade, CI). Prefira soluções que exercitem isso.
- Editor: LazyVim (Neovim) + clangd. Não sugerir fluxos dependentes de
  VS Code ou de IDEs gráficas.
- SO: Arch Linux (Omarchy). Grupo da serial é `uucp`. Porta típica:
  `/dev/ttyUSB0`.

## Hardware

| Item | Modelo | Observações |
|---|---|---|
| MCU (ambos os nós) | ESP32 WROOM-32 DevKit (30 pinos) | Alimentação via USB |
| Rádio | LoRa Ra-02 (SX1278), 433 MHz | SPI, 3.3 V, NUNCA transmitir sem antena |
| Temp/pressão | BMP280 | I²C, endereço 0x76 ou 0x77 |
| Umidade | DHT11 | Protocolo single-wire proprietário |
| Luminosidade | LDR KY-018 | Saída analógica (divisor de tensão) |
| Chuva | MH-RD | AO (intensidade) + DO (limiar) |
| Vento | Reed-switch + ímã | Pulsos; pull-up interno + debounce |
| Status | LED RGB | 3 canais PWM (LEDC) |

## Mapa de pinos do nó externo (NÃO alterar sem discutir)

| Função | GPIO | Nota |
|---|---|---|
| I²C SDA (BMP280) | 21 | |
| I²C SCL (BMP280) | 22 | |
| DHT11 dados | 4 | |
| LDR AO | 34 | ADC1_CH6, somente entrada |
| MH-RD AO | 35 | ADC1_CH7, somente entrada |
| MH-RD DO | 27 | opcional |
| Reed-switch | 25 | interrupção + pull-up interno + debounce |
| LoRa SCK | 18 | VSPI |
| LoRa MISO | 19 | VSPI |
| LoRa MOSI | 23 | VSPI |
| LoRa NSS (CS) | 5 | |
| LoRa RST | 14 | |
| LoRa DIO0 | 26 | IRQ de TX done / RX done |
| LED R / G / B | 16 / 17 / 13 | PWM via LEDC |

Regras de pinagem: nunca usar GPIO 6–11 (flash) nem 0, 2, 12, 15
(strapping). Todo analógico fica no ADC1 (o ADC2 conflita com Wi-Fi).
Não usar GPIO 1/3 (UART0 — monitor serial e gravação). A justificativa
completa de cada escolha está em `docs/pinagem.md`.

Nota do silkscreen da placa: GPIO 16 e 17 aparecem como `RX2` e `TX2`.

## Ambiente de build (Docker — decisão consciente do projeto)

Toolchain 100% dockerizado com a imagem oficial, orquestrado pelo
**Makefile na raiz** (não usar alias). Comandos principais:

```bash
make build            # compila (não exige a placa conectada)
make flash            # grava
make monitor          # serial (sair: Ctrl+])
make run              # flash + monitor
make menuconfig       # configuração do projeto
make build PROJ=firmware/gateway   # compila o outro nó
```

O Makefile monta o repositório no MESMO caminho dentro do container,
para que os caminhos do `compile_commands.json` valham no host.

### LSP (clangd no LazyVim)

- Uma única vez: `make lsp-setup` — extrai o ESP-IDF e o toolchain da
  imagem para `/opt/esp` no host (~3 GB), fazendo os caminhos de headers
  referenciados no `compile_commands.json` existirem fora do container.
- O arquivo `.clangd` na raiz remove flags exclusivas do GCC Xtensa que
  o clangd não conhece (`-mlongcalls` etc.). Não apagar.
- Se headers da libc (stdint.h etc.) não resolverem, configurar o clangd
  no LazyVim com
  `--query-driver=/opt/esp/tools/**/xtensa-esp32-elf-gcc`.
- O clangd encontra o `compile_commands.json` sozinho (procura em
  subpastas `build/` dos ancestrais do arquivo aberto) — não é preciso
  symlink.

## Convenções de trabalho com o Claude Code

1. **Modo professor**: antes de implementar, explicar o conceito
   (o periférico, o protocolo, a API do IDF envolvida) em 1–3 parágrafos.
2. **Uma mudança por vez**: cada passo termina com algo verificável no
   monitor serial. Nunca integrar dois componentes novos no mesmo passo.
3. **Linguagem**: C (padrão do ESP-IDF v5.x), com FreeRTOS. Usar
   `ESP_LOGx` para logs, `ESP_ERROR_CHECK` para retornos, nunca busy-wait
   (sempre `vTaskDelay`).
4. **Documentação contínua**: todo passo concluído gera/atualiza um
   arquivo em `docs/` (ex.: `docs/02-bmp280.md`) com: o que foi feito,
   por que, comandos usados, problemas encontrados e como validar.
5. **Git**: commits pequenos e descritivos, um por passo concluído.
6. **Não inventar**: se um registrador, endereço ou API for incerto,
   consultar o datasheet/documentação em vez de chutar.
7. Seguir as fases do `PLANO.md` em ordem; não pular critérios de
   aceitação.

## Estrutura de diretórios alvo

```
.
├── CLAUDE.md
├── PLANO.md
├── README.md              # documento da disciplina
├── docs/                  # diário de bordo por etapa
├── firmware/
│   ├── estacao/           # projeto ESP-IDF do nó externo
│   └── gateway/           # projeto ESP-IDF do nó base
├── infra/                 # docker-compose (mosquitto, dashboard), CI
└── common/                # código compartilhado (formato do pacote LoRa)
```
