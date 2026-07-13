# 08 — LoRa Ra-02 (SX1278): bring-up do SPI

## Mudança de rota da Fase 4 (decisão de arquitetura)

Só existe **um** módulo Ra-02. Os módulos verdes que iriam para o segundo
nó (FS1000A + MX-RM-5V) são transmissor/receptor **ASK/OOK** de 433 MHz —
operam na mesma frequência, mas não falam LoRa (CSS/chirp): frequência
igual não é idioma igual. Sem tempo hábil para um segundo Ra-02, o
receptor do enlace passa a ser o **PC com RTL-SDR v3** (RTL2832U + R860,
TCXO):

- validação visual do TX: chirps no waterfall (gqrx);
- decodificação real: `gr-lora_sdr` (GNU Radio, tem imagem Docker — o
  gateway vira um compose de decoder + broker + banco + dashboard, o que
  reforça o objetivo DevOps);
- o MQTT **não** se perde: quem publica no Mosquitto passa a ser o PC.

O segundo ESP32 sai do caminho do rádio. Os conversores de nível também
(eram para os módulos de 5 V; o Ra-02 é 3.3 V ponta a ponta).

## O que foi feito neste passo

Driver SPI próprio e mínimo (`main/lora.c`), decisão consciente do plano
para aprender SPI em vez de já integrar componente pronto:

- **VSPI @ 1 MHz** (SCK 18, MISO 19, MOSI 23, NSS 5), modo 0. 1 MHz é
  conservador de propósito: jumpers em protoboard; o SX1278 aceita 10 MHz.
- O driver `spi_master` do IDF conduz o **NSS sozinho** (`spics_io_num`).
- **Reset por hardware** no RST (GPIO 14): nível baixo >= 100 µs, solta,
  ~5 ms até o chip ficar pronto (datasheet 7.2.2).
- Leitura de registrador: transação full-duplex de 2 bytes; 1º byte =
  endereço com MSB 0 (leitura), o valor volta pelo MISO no 2º byte.
- Novo modo de teste no menuconfig (`ESTACAO_TESTE_LORA`): lê o
  **RegVersion (0x42)** a cada 2 s e compara com **0x12** (valor gravado
  em silício — o "quem é você?" do chip).

Ler 0x12 prova a cadeia inteira: 3V3, GND, SCK, MOSI, MISO, NSS e RST.
Não há nenhum código de TX ainda — leitura de registrador não emite RF,
por isso este passo pôde rodar **sem antena**.

## Fiação (Ra-02 → ESP32)

| Ra-02 | ESP32 | Função |
|---|---|---|
| 3.3V | 3V3 | nunca no VIN/5V |
| GND ×2 | GND | segundo GND reforça o retorno de RF |
| SCK | D18 | clock |
| MISO | D19 | rádio → ESP32 (ligar nome com nome) |
| MOSI | D23 | ESP32 → rádio |
| NSS | D5 | chip select |
| RST | D14 | reset |
| DIO0 | D26 | IRQ TxDone/RxDone (para o próximo passo) |

DIO1–DIO5 ficam desconectados.

## Problemas encontrados

1. **Leitura intermitente** (0x12 ↔ 0x00 ao longo de dezenas de
   segundos): assinatura de contato, não de software (erro de config de
   SPI seria determinístico). Causa raiz: **MOSI espetado num furo vazio**
   da protoboard — o endereço do registrador chegava ao rádio só por
   acoplamento fraco, daí os acertos ocasionais.
2. Diagnóstico por software que ajudou: **pull-up interno no MISO**.
   Tabela de leitura: `0x12` ok; `0xFF` = rádio energizado mas
   MISO/NSS abertos; `0x00` = alimentação do rádio caindo (chip apagado
   afunda a linha pelos diodos de proteção). Ficou permanente no driver.
3. **A novela dos conectores** (resolvida): a suspeita de RP-SMA no
   pigtail veio de uma leitura errada do miolo. Inventário final:
   pigtail SMA **macho** (pino), antena SMA **macho** (pino), BPF SMA
   **fêmea** dos dois lados. Como os dois machos não se emendam, o BPF
   (sem serigrafia, banda desconhecida) virou o "adaptador" candidato —
   e a cadeia `Ra-02 → pigtail → BPF → antena` ficou pino-no-encaixe
   nas duas junções. Lição: rosca engatada não prova contato; o miolo
   (pino vs furo) é que decide, e RP-SMA existe para confundir.
4. **Potência mínima como política de bancada**: +2 dBm fixo no código.
   Torna inofensivo transmitir mesmo com a cadeia de antena incerta
   (a regra "nada de TX sem antena" protege o PA em potência alta) e
   transformou o próprio TX no instrumento de medida do BPF.

## TX validado no SDR (marco da fase)

Segundo passo do driver: `lora_config_modem()` + `lora_tx()`.

- Modem: **433,0 MHz** (`RegFrf = 0x6C4000`; longe dos controles de
  portão em 433,92), **SF12** didático (símbolo de 32,8 ms), BW 125 kHz,
  CR 4/5, CRC on, LowDataRateOptimize (obrigatório com símbolo > 16 ms),
  **PA_BOOST a +2 dBm** (única saída ligada à antena no Ra-02).
- TX: payload na FIFO (`FifoTxBase`/`FifoAddrPtr`), `PayloadLength`,
  modo TX e **polling do bit TxDone** em `RegIrqFlags` (o DIO0 fica
  para o próximo passo — uma coisa nova por vez). Payload "chirp #N" a
  cada 5 s; TxDone medido em ~990 ms, batendo com a fórmula de tempo
  no ar do datasheet para SF12.
- Validação no gqrx (RTL-SDR v3 + antena; `rtl_test` ok de primeira):
  faixas de **125 kHz de largura**, centradas em **433,000 MHz**,
  durando **~1 s**, repetindo a **cada 5 s** — os 4 criterios
  (largura, frequência, duração, periodicidade) fechados.
- Bônus: o sinal atravessou o BPF com força ⇒ **o BPF passa 433 MHz**,
  aprovado como emenda definitiva entre pigtail e antena.

## Como validar

```
make menuconfig   # Estacao - teste de bancada -> LoRa Ra-02 (bring-up ou TX)
make run
```

Bring-up: `RegVersion=0x12` estável por minutos. TX: `TxDone em ~990 ms`
a cada 5 s + as faixas no waterfall do gqrx em 433,0 MHz.

## Decodificação no PC: gr-lora_sdr em Docker (risco eliminado)

Pipeline completo validado: `chirp #N` impresso no PC com header e CRC
válidos, sequência sem furos. O gateway agora é software:

- `infra/sdr/Dockerfile`: Ubuntu 24.04 (GNU Radio 3.10.9 do apt) +
  gr-osmosdr + `gr-lora_sdr` compilado da fonte, **fixado por commit**
  (`862746d` — o projeto não publica releases). Pin = build de véspera
  de apresentação não quebra.
- `infra/sdr/receptor_lora.py`: réplica do exemplo oficial
  (`examples/lora_RX.py`) trocando o USRP por RTL-SDR via osmosdr.
  Parâmetros por variável de ambiente, casados 1:1 com o firmware.
  LDRO calculado pelo critério do datasheet (símbolo > 16 ms), igual
  nas duas pontas.
- `infra/docker-compose.yml`: serviço `sdr-receptor` (privileged +
  /dev/bus/usb para o dongle; refinável com device_cgroup_rules).

### As três batalhas do debug (para não esquecer)

1. **Python invisível no Ubuntu 24.04**: o cmake instala em
   `site-packages`, Debian/Ubuntu só leem `dist-packages` →
   `-DGR_PYTHON_DIR=/usr/lib/python3/dist-packages`.
2. **SF12 estoura o buffer do GNU Radio**: o `frame_sync` pede um
   símbolo inteiro por chamada (2^12 × os_factor + 4 = 8196 amostras)
   e o buffer padrão oferece 8191 → flowgraph morre no boot (é o
   issue #55 do gr-lora_sdr, aberto sem resposta).
3. **A descoberta**: `set_min_output_buffer(N)` (forma de 1 argumento,
   "todas as portas") é **silenciosamente ignorada** pelo binding
   Python do GR 3.10; `set_min_output_buffer(0, N)` (porta explícita)
   funciona. Provado por experimento controlado no container (fonte
   nula, variantes lado a lado, sem hardware). Provavelmente é por
   isso que o issue #55 segue aberto — o conserto "óbvio" usa a forma
   quebrada. Fica um bloco `blocks.copy` na entrada só para portar o
   buffer de 4 símbolos.

### Como rodar

```bash
# estacao transmitindo (modo TX de teste) + dongle plugado, gqrx FECHADO
cd infra && docker compose up sdr-receptor
# esperado: Header (len/CRC/CR) + "rx msg: chirp #N" + "CRC valid!" a cada 5 s
```

## Próximos passos

1. Payload real: substituir o texto pelo pacote de 18 B da Fase 3
   (CRC16 + seq); `LORA_FORMATO=hex` no compose; validar no PC.
2. Voltar para SF7/BW125 (o enlace de produção) e medir perda via seq.

Referências: Semtech **AN1200.22** (LoRa Modulation Basics), datasheet
SX1276/77/78/79 cap. 4.1, calculadora de airtime do TTN.
