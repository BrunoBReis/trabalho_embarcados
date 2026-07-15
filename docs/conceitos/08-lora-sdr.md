# Conceitos — Etapa 08: LoRa (SX1278) + SDR

Síntese das discussões de revisão da etapa 08 (diário:
`docs/diario_bordo/08-lora.md`, código: `firmware/estacao/main/lora.c`,
`infra/sdr/Dockerfile`, `infra/sdr/receptor_lora.py`).

## A modulação: chirp, CSS e LoRa

### Chirp

Um **chirp** é um sinal cuja frequência **varre continuamente** uma faixa
ao longo do tempo (*up-chirp*: sobe da borda de baixo à de cima da banda;
*down-chirp*: o inverso). O nome é onomatopeia do canto de pássaro — um
assobio subindo de tom.

O CSS carrega informação na **posição de início da varredura**: o símbolo
dura `T = 2^SF / BW` (SF12/125 kHz: 4096/125000 = **32,8 ms**) e pode
começar em 2^SF pontos da banda — 4096 posições em SF12 = **12 bits por
símbolo**. O chirp começa na frequência correspondente ao valor, sobe até a
borda e "dá a volta" (deslocamento cíclico do chirp base).

Por que funciona tão bem:

1. **Demodulação trivial** ("dechirp"): multiplicar pelo down-chirp
   conjugado cancela a varredura; o chirp deslocado vira um **tom
   constante** e o índice do pico da FFT **é** o símbolo. É exatamente o
   bloco `fft_demod` do receptor.
2. **Espectro espalhado**: cada símbolo ocupa a banda toda por um tempo
   longo — interferência estreita e ruído se diluem; LoRa decodifica
   abaixo do piso de ruído (por isso +2 dBm sobram na bancada).
3. **Tolerância a deriva de cristal/Doppler**: erro de frequência desloca
   o pico ±1 bin; o código de Gray transforma isso em 1 bit errado, que o
   Hamming corrige.

No waterfall do gqrx, cada faixa de 125 kHz × ~1 s é o quadro inteiro:
8 up-chirps de preâmbulo (símbolo 0 repetido — o padrão que o `frame_sync`
procura), sync word, 2,25 down-chirps, header e payload.

### LoRa vs CSS (gênero e espécie)

- **CSS** (Chirp Spread Spectrum) é a *técnica de modulação* — conceito
  público, usado em radar desde os anos 1940.
- **LoRa** é a *implementação proprietária da Semtech* de um PHY sobre
  CSS: preâmbulo, sync word, mapeamento símbolo→deslocamento cíclico,
  código de Gray, entrelaçamento, Hamming, whitening, header e CRC.
- **LoRaWAN** é outra camada acima (MAC/rede sobre o PHY LoRa). O projeto
  usa LoRa "cru", sem LoRaWAN.

Analogia: CSS está para o LoRa como "serial síncrona" está para o SPI —
princípio físico vs protocolo completo.

## O lado SDR

### gr-lora_sdr e GNU Radio

**GNU Radio**: framework de rádio definido por software — monta-se um
*flowgraph* (grafo de blocos de processamento: fonte → filtros →
demodulador) e o runtime C++ (com bindings Python) empurra amostras entre
blocos, uma thread por bloco.

**gr-lora_sdr**: módulo *out-of-tree* (OOT) do GNU Radio — blocos extras
instalados por fora, visíveis como `gnuradio.lora_sdr`. Desenvolvido no
laboratório TCL da EPFL (Tapparel et al.), é uma implementação **aberta e
completa do PHY LoRa em software** — notável porque o LoRa é proprietário
(nasceu de engenharia reversa/publicações). Todo o pipeline que existe em
silício no SX1278 refeito como blocos de flowgraph.

**Por que ajuda aqui**: com um único Ra-02, ninguém "fala LoRa" para
receber o pacote. O RTL-SDR + gr-lora_sdr fazem o papel do segundo SX1278:
o gateway de hardware virou software no PC.

### Docker nesta etapa

O caso mais forte de DevOps do projeto, porque a dependência é hostil:

1. gr-lora_sdr **não tem release nem pacote** — compila da fonte contra uma
   versão específica do GNU Radio. No Arch (rolling), o GR do sistema muda;
   no container é o 3.10.9 do Ubuntu 24.04 para sempre (o 22.04 traz o
   3.10.1, que tem o bug do `min_output_buffer` que quebra SF12).
2. **Reprodutibilidade**: imagem congela distro + GNU Radio + commit exato
   do módulo; `docker compose up sdr-receptor` reconstrói o receptor
   idêntico em qualquer máquina.
3. **Não polui o host**: os `.so` e módulos Python ficam na imagem.
4. **Integração**: o receptor é um serviço no compose ao lado do Mosquitto
   (a rede do compose resolve `MQTT_HOST=mosquitto` por nome).

### `ARG GR_LORA_SDR_REF=<hash>` — pin por commit

`ARG` é variável de build do Docker; o valor é um hash de commit usado no
`git checkout`. **Fixa (pin)** o gr-lora_sdr num commit exato: sem isso, o
clone pegaria "o main de hoje" e o build de véspera de apresentação poderia
quebrar. É o análogo, para dependência via git, do `dependencies.lock` do
Component Manager (etapa 01). Por ser `ARG`, dá para testar outro commit
com `--build-arg` sem editar o arquivo.

### Da antena ao payload (o caminho completo)

É o TX do firmware desfeito em ordem inversa:

1. **Antena → RTL-SDR**: o tuner (R860) translada a fatia do espectro em
   433,0 MHz para banda-base; o RTL2832U digitaliza — amostras IQ
   (complexas, 8 bits) a 250 kS/s pela USB. Daqui em diante é só software.
2. **`osmosdr.source`**: bloco-fonte; frequência, taxa, ganho.
3. **`blocks.copy`**: não processa — só carrega um buffer maior (4
   símbolos), workaround do issue #55 (SF12 pede 8196 amostras/chamada;
   buffer padrão dá 8191). ATENÇÃO: usar `set_min_output_buffer(0, N)` —
   a forma de 1 argumento é silenciosamente ignorada no GR 3.10.
4. **`frame_sync`**: acha o preâmbulo (8 up-chirps), reconhece a sync word
   e alinha tempo/frequência (corrige offset de oscilador).
5. **`fft_demod`**: dechirp + FFT — pico da FFT = símbolo.
6. **`gray_mapping`**: desfaz o código de Gray (erro de ±1 bin → 1 bit).
7. **`deinterleaver`**: desfaz o entrelaçamento (o TX espalha os bits de
   cada byte por vários símbolos → perda vira erros espalhados que o FEC
   corrige, não um buraco concentrado).
8. **`hamming_dec`**: o FEC do CR 4/5 (1 bit de paridade a cada 4).
9. **`header_decoder`**: lê o header explícito (tamanho/CR/CRC) e
   realimenta o `frame_sync` via porta de mensagem `frame_info` — é assim
   que o RX sabe quantos símbolos faltam.
10. **`dewhitening`**: desfaz o embaralhamento pseudo-aleatório.
11. **`crc_verif`**: confere o **CRC do quadro LoRa** (PHY) e publica o
    payload como mensagem.
12. **`PonteMqtt`** (código nosso): valida o CRC16 fim-a-fim, desserializa
    os 18 B e publica JSON no Mosquitto.

### O BPF (filtro passa-faixa)

Usado mecanicamente como emenda fêmea-fêmea entre dois SMA machos, mas
eletricamente ele **passa uma janela de frequências e atenua o resto**:

- **No TX**: atenua as harmônicas do PA (866/1299 MHz); o sinal desejado
  atravessa com perda de inserção pequena (~1–2 dB).
- **No RX** (se estivesse na antena do SDR): protegeria o front-end do
  RTL-SDR de sinais fortes fora de banda (FM, TV, celular), que o saturam
  mesmo fora da frequência sintonizada.

O do projeto veio **sem serigrafia** (banda desconhecida) e foi validado
experimentalmente: o TX a +2 dBm o atravessou com força ⇒ passa 433 MHz.
Não é peça neutra — um BPF de outra banda mataria o sinal (30–40 dB).

## O driver no firmware (`lora.c`)

### SPI a 1 MHz — efeito prático

- **Elétrico (motivo real)**: jumpers de protoboard têm parasitas e
  contatos duvidosos; a 1 MHz cada bit dura 1 µs e a linha assenta. Margem
  de robustez (houve caso real: MOSI em furo vazio).
- **Tempo (irrelevante)**: transação de registrador = 16 µs; FIFO de 19 B ≈
  152 µs — contra ~1,3 s de pacote no ar em SF12, o SPI é 4 ordens de
  grandeza mais rápido que o gargalo. Subir a 10 MHz não mudaria nada.

### As três configs do `lora_init()`

- **`gpio_config_t`**: `.pin_bit_mask = 1ULL << PIN` — a API configura
  vários pinos de uma vez via máscara de 64 bits (`ULL` porque há GPIOs >
  31); `.mode = OUTPUT` porque nós dirigimos o RST.
- **`spi_bus_config_t`** (o barramento): roteia SCK/MISO/MOSI pela matriz
  de GPIO; `quadwp/quadhd = -1` desliga os pinos do modo quad-SPI (4 linhas
  de dados, coisa de memória flash); `SPI_DMA_CH_AUTO` deixa o driver
  escolher canal de DMA.
- **`spi_device_interface_config_t`** (o dispositivo no barramento):
  `clock_speed_hz` por dispositivo; `.mode = 0` = CPOL/CPHA (clock repousa
  em baixo, amostra na subida — tem que casar com o chip);
  `.spics_io_num` entrega o NSS ao driver (abaixa no início da transação,
  sobe no fim); `.queue_size` = fila interna para uso assíncrono.

### Reset por hardware

```c
gpio_set_level(RST, 0); vTaskDelay(2ms);   // segura RST (ativo-baixo)
gpio_set_level(RST, 1); vTaskDelay(10ms);  // solta e espera acordar
```

Datasheet §7.2.2 (pede ≥100 µs baixo e ~5 ms de espera; os valores têm
folga). Força o chip ao estado de power-on. Necessário porque o ESP32 pode
rebootar **sem** cortar a alimentação do rádio — sem reset, o rádio
continuaria no estado da execução anterior e o firmware partiria de
premissas falsas.

### Leitura/escrita de registrador — o full-duplex do SPI

Protocolo do SX1278: **1º byte = comando** (7 bits de endereço + bit
**wnr**: MSB 0 = leitura, 1 = escrita), **2º byte = dado**.

- `reg & 0x7F` (AND com `0111 1111`): força MSB=0 → leitura.
- `reg | 0x80` (OR com `1000 0000`): força MSB=1 → escrita.

O pulo do gato: **SPI é full-duplex e "burro"** — para cada bit que entra
por MOSI, um sai por MISO no mesmo pulso de clock. Não há
"pergunta-depois-resposta":

```
clock:        [ byte 1 ]          [ byte 2 ]
MOSI (envia): endereço|wnr=0      0x00 (sacrifício, só gera clock)
MISO (volta): lixo (rx[0])        VALOR DO REGISTRADOR (rx[1])
```

Durante o byte 1 o rádio ainda está ouvindo o endereço (devolve lixo); no
byte 2, enquanto o master transmite um 0x00 ignorado (só o master gera
clock), o rádio põe o registrador no MISO. Por isso `*valor = rx[1]` — o
dado útil é o **segundo** byte recebido; `valor` é parâmetro de saída
(idioma C: erro pelo `return`, dado pelo ponteiro). Na escrita não há nada
a receber — `escrever_reg` nem tem `rx_buffer`.

Diagnóstico embutido: pull-up interno no MISO → leitura `0xFF` = linha
aberta (fio/NSS); `0x00` persistente = alimentação do rádio caindo.

### `PA_CONFIG_BOOST_2DBM 0x80`

`RegPaConfig` (0x09), `0x80 = 1000 0000`:

- **bit 7 (PaSelect) = 1**: saída **PA_BOOST** — no Ra-02 é a única
  fisicamente ligada ao conector de antena (RFO está no ar). Imposição do
  módulo, não escolha.
- **bits 3–0 (OutputPower) = 0**: `Pout = 2 + OutputPower` → **+2 dBm
  (~1,6 mW)**, o mínimo.

Política de bancada: (1) PA transmitindo em carga ruim/aberta reflete
potência de volta — a +2 dBm é inofensivo com cadeia de antena não
comprovada; (2) alcance de sobra até o SDR; (3) com potência fixa e
conhecida, o TX virou instrumento de medida do BPF.

### A FIFO — desfazendo a imagem de "fila"

A "FIFO" do SX1278 **não** é fila de mensagens que acumula pedidos: é um
**bloco de 256 B de RAM com ponteiro auto-incrementante** — buffer de
rascunho de **um pacote por vez**. Fluxo:

1. `FifoAddrPtr = FifoTxBase` (cursor no início da área de TX);
2. despeja N bytes em `REG_FIFO` — o cursor avança sozinho (o
   `escrever_fifo` manda o endereço uma vez e os bytes em rajada);
3. `PayloadLength = N`;
4. `OPMODE_TX` — só agora o modem lê a RAM, monta o quadro (preâmbulo +
   header + payload + CRC) e transmite;
5. TxDone → FIFO livre para o próximo.

Quem controla: o firmware escreve via SPI; o modem lê quando comandado.
Não há limiar de acúmulo nem envio automático. Os 256 B são compartilhados
TX/RX (default: metade para cada); o driver põe `FIFO_TX_BASE = 0x00` para
dar tudo ao TX, já que este firmware não recebe.

### Modos de operação (RegOpMode, 3 bits baixos)

| Modo | Bits | O que faz |
|---|---|---|
| SLEEP | 000 | tudo desligado; **único** em que se troca FSK↔LoRa |
| STDBY | 001 | osciladores ligados, ocioso — onde se configura |
| FSTX | 010 | sintetizador ligando p/ TX (transitório, automático) |
| TX | 011 | transmite; ao fim **volta a STDBY sozinho** + TxDone |
| FSRX | 100 | sintetizador ligando p/ RX (transitório) |
| RXCONTINUOUS | 101 | recebe sem parar |
| RXSINGLE | 110 | recebe um quadro ou dá timeout |
| CAD | 111 | detecção rápida de preâmbulo gastando quase nada |

O firmware usa SLEEP, STDBY e TX; os modos RX ficam com o GNU Radio.

### `lora_config_modem()` linha a linha

- `OPMODE_LORA_LF (0x88)`: LongRangeMode=1 (LoRa) + LowFrequencyModeOn
  (banda 433) + SLEEP — o bit LoRa **só é aceito em SLEEP**.
- `REG_FRF` ×3: portadora em 24 bits, passo `Fxosc/2^19` ≈ 61 Hz;
  `433e6 × 2^19 / 32e6 = 0x6C4000`. Shifts + `& 0xFF` fatiam em
  MSB/MID/LSB. 433,0**0** de propósito (longe dos portões em 433,92).
- `REG_PA_CONFIG = 0x80`: PA_BOOST, +2 dBm (acima).
- `MODEM_CONFIG1 = 0x72` (`0111 001 0`): BW 125 kHz | CR 4/5 | header
  **explícito** (o quadro carrega tamanho/CR/CRC — é de onde o
  `header_decoder` do RX tira tudo).
- `MODEM_CONFIG2 = 0xC4` (`1100 x1xx`): **SF12** (símbolo de 32,8 ms,
  didático — chirps visíveis no waterfall) | CRC do payload (PHY) ligado.
- `MODEM_CONFIG3 = 0x0C`: **LDRO** (obrigatório com símbolo > 16 ms — a
  deriva dos cristais acumularia dentro do símbolo; sacrifica 2 bits/símbolo
  por margem) + AgcAutoOn. **LDRO não viaja no header** — as pontas
  combinam por fora (o Python espelha o critério do datasheet).
- `OPMODE_STANDBY`: configurado, pronto para o primeiro TX.

### `lora_tx()` passo a passo

1. valida `len` (1–32; limite do buffer local `tx[33]`);
2. STANDBY defensivo;
3. ritual da FIFO (base, cursor, rajada, PayloadLength);
4. `IRQ_FLAGS = 0xFF` — limpa flags pendentes (**write-1-to-clear**:
   escrever 1 apaga o bit) para não confundir TxDone velho com novo;
5. `OPMODE_TX` — o silício trabalha sozinho (~1,3 s em SF12) e volta a
   STANDBY com TxDone ligado;
6. **polling educado**: a cada 10 ms (`vTaskDelay` — nunca busy-wait) lê
   `RegIrqFlags` e testa `irq & IRQ_TX_DONE` (AND como *teste* de bit);
   timeout de 5 s;
7. limpa flags de novo e devolve o tempo em `*ms_no_ar` (~990 ms, batendo
   com a fórmula do datasheet).

Alternativa ao polling: o pino DIO0 pulsa no TxDone (interrupção). Fiado
(GPIO 26) mas não usado — "uma coisa nova por vez".

### Máximo de informação por pacote

1. **Protocolo**: campo de tamanho (registrador e header) tem 8 bits →
   **255 B**;
2. **Hardware**: FIFO de 256 B — payload inteiro precisa caber;
3. **Driver do projeto**: 32 B (buffer local); o pacote usa 18;
4. **Prática**: tempo no ar — 18 B ≈ 1,3 s em SF12; 255 B ≈ 9 s
   (canal ocupado, energia, duty cycle regulatório das bandas ISM). LoRa é
   para pacotes pequenos e raros.

### Bibliotecas prontas (vs driver na mão)

Existem: **RadioLib** (a mais completa; HAL portável com exemplos
ESP-IDF), **dernasherbrezon/sx127x** (componente nativo do IDF, no
Component Registry), **nopnop2002/esp-idf-sx127x** (porte simples). Dariam
RX, DIO0, mais modos. Mas o driver na mão foi decisão consciente do plano
(aprender SPI e a anatomia do rádio) e, para TX de 18 B a cada 10 s, é
suficiente e auditável ponta a ponta. Num produto: RadioLib ou o sx127x do
registry.

## O contrato entre as pontas

### Sync word 0x12

Byte após o preâmbulo, **identificador de rede**: o RX descarta quadros de
sync word diferente antes de decodificar. 0x12 = default do chip ("rede
privada" — o firmware nunca escreve `RegSyncWord`, vale o reset default);
0x34 = LoRaWAN público. Divergência entre as pontas = o `frame_sync` não
reconhece nada. Não é segurança; é filtro de coexistência.

### Dois CRCs por design + a duplicação C/Python

Dois CRCs **diferentes com papéis diferentes**:

- **CRC do quadro LoRa (PHY)**: ligado no `MODEM_CONFIG2`, conferido pelo
  bloco `crc_verif` **da biblioteca** (o "CRC valid!"). Protege só o trecho
  aéreo.
- **CRC16-CCITT fim-a-fim do payload**: calculado em
  `common/pacote/crc16.c` (TX) e conferido pelo `desserializar()` **do
  nosso código** (RX). Continua verificável depois do MQTT e do banco.

A implementação existe **duas vezes** (C e Python) porque as pontas rodam
em runtimes que não compartilham código (Xtensa bare-metal vs CPython em
container). Centralizar (ctypes/cffi, geração de código) custaria mais que
10 linhas de algoritmo valem. A proteção contra divergência é **um vetor de
teste compartilhado**: `crc16("123456789") == 0x29B1` (valor canônico do
CRC16-CCITT-FALSE), verificado no boot das duas pontas
(`pacote_selfcheck()` no firmware, `assert` no `main()` do Python). **O
contrato não é o código — é o vetor de teste.**

### `crc_verif(2, False)`

Bloco da biblioteca: confere o CRC do PHY; `2` = imprime o payload em hex
no stdout (conferência lado a lado com o `ESP_LOG_BUFFER_HEX` da estação);
`False` = não anexa o resultado à saída. "A ponte é quem valida de
verdade, via CRC16 próprio."

### `payload_de_pmt()` — resgatando binário de uma "string"

**PMT** (Polymorphic Type) é o formato de mensagens entre blocos do GNU
Radio. O `crc_verif` publica o payload como **símbolo PMT** (string C++);
converter com `str()` assumiria UTF-8 e corromperia/explodiria com os 18
bytes binários. `pmt.serialize_str(msg)` devolve o **formato de fio** do
PMT sem interpretar nada: `[opcode 1 B][tamanho u16 big-endian][bytes
crus]` — a função pula o opcode, lê o tamanho, fatia e valida. Contorno de
fronteira de linguagem: C++ guardou binário numa "string", o Python o
resgata sem passar por decodificação de texto.

### Variáveis de ambiente ≠ `ldconfig`

- **`ldconfig`** só atualiza o **cache do linker dinâmico**
  (`/etc/ld.so.cache`) depois que o `cmake --install` despejou `.so` novos
  — é o que faz o Python achar `libgnuradio-lora_sdr.so` em runtime.
- **As variáveis** (`LORA_SF`, `MQTT_HOST`...) vêm do
  `docker-compose.yml` (seção `environment:`) ou dos defaults do próprio
  código (`os.environ.get("LORA_SF", 12)`). Precedência: default no código
  ← `ENV` do Dockerfile ← `environment` do compose.

### `signal.signal` ≠ sinal de rádio

Colisão de vocabulário: são **sinais do sistema operacional** — `SIGINT` =
Ctrl+C, `SIGTERM` = o que o `docker compose down` manda. Registram
`parar()` para desligamento limpo (`tb.stop()` → `tb.wait()` →
`loop_stop()`). O sinal de RF entra por outro lugar: o bloco
**`osmosdr.source`** (via libusb).

### `tb.run()`

Implementado no **GNU Radio** (`gr.top_block` — binding fino sobre o
runtime **C++**). Faz `start()` + `wait()`: o `start()` materializa o grafo
(aloca buffers — honrando o `set_min_output_buffer` — e sobe uma thread
por bloco); o `wait()` bloqueia até o flowgraph parar. Como a fonte é um
rádio que nunca "acaba", `run()` é o loop principal do serviço — quem o
encerra é o `tb.stop()` do handler de SIGTERM. O trabalho pesado roda nas
threads C++; o Python só montou o grafo e trata as mensagens da ponte.

### Papel do `receptor_lora.py`

**O gateway inteiro em um arquivo**: (1) monta e roda o flowgraph (réplica
do `lora_RX.py` oficial com RTL-SDR via osmosdr + workaround do buffer);
(2) espelha o contrato do modem por variáveis de ambiente casadas 1:1 com o
`lora.c` (inclusive LDRO pelo mesmo critério); (3) ponte para o mundo IP —
valida CRC16, desserializa os 18 B e publica JSON no Mosquitto com retain.
Rádio entra, JSON sai.

### `TESTE_TODOS` vs `MODO_ESTACAO`

| | TESTE_TODOS (bancada) | MODO_ESTACAO (produção) |
|---|---|---|
| Objetivo | validar hardware no serial | operar |
| Ciclo | fixo 5 s | `CONFIG_ESTACAO_INTERVALO_S` (5–3600 s) |
| Saída | log legível | `pacote_t` de 18 B com flags e CRC |
| Rádio | só o autoteste do boot | **transmite** cada pacote |

Bancada = "mostre tudo, não transmita"; estação = "empacote e transmita".
Ambos compartilham o `autoteste()` do boot.

### Flag de erro do reed (vento): não existe, de propósito

`pacote.flags` só tem bits para BMP280/LDR/chuva/DHT11. O reed é contador
de pulsos por interrupção — **não há modo de falha detectável em
runtime**: `sensor_vento_coletar_pulsos()` nem retorna `esp_err_t`; fio
solto = 0 pulsos = indistinguível de calmaria. No `[SELFTEST]`, `vento OK`
atesta só a inicialização (GPIO + ISR). Bits 4–7 estão reservados, mas uma
flag nunca ligaria na prática.
