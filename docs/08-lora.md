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
3. **Pendência (bloqueia o TX)**: pigtail IPEX→SMA e antena são ambos
   SMA macho; falta adaptador fêmea-fêmea. Candidatos: o BPF (se for da
   banda de 433 MHz — conferir a serigrafia; SAW aguenta pouca potência,
   então TX no mínimo +2 dBm) ou adaptador do laboratório. Conferir
   também se os conectores não são RP-SMA (miolo: pino vs furo).
   **Regra: nenhum código de TX antes de antena resolvida.**

## Como validar

```
make menuconfig   # Estacao - teste de bancada -> LoRa Ra-02
make run
```

Esperado, estável por minutos: `RegVersion=0x12 — SX1278 respondendo`.

## Próximos passos

1. Resolver antena (adaptador ou BPF 433 MHz) — TX no mínimo (+2 dBm).
2. Configurar o modem: 433,0 MHz (longe dos controles de portão em
   433,92), SF12/BW125 "didático" (símbolo de 32,8 ms — chirps visíveis
   a olho nu no waterfall; pacote de 18 B ≈ 1,3 s no ar).
3. TX com TxDone via DIO0; validar no gqrx pelos 4 critérios: instante
   (bate com o log serial), frequência central, largura (125 kHz) e
   "obediência" (mudar freq/BW/SF no firmware e ver o sinal acompanhar).
4. `gr-lora_sdr` para decodificar; depois voltar a SF7/BW125.

Referências: Semtech **AN1200.22** (LoRa Modulation Basics), datasheet
SX1276/77/78/79 cap. 4.1, calculadora de airtime do TTN.
