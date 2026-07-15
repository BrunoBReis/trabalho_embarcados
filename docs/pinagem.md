# Pinagem da ESP32 — decisões e fundamentos

## 1. O modelo mental: três camadas físicas

Para entender qualquer coisa sobre pinos, é preciso separar três objetos
que costumam ser chamados todos de "a ESP32":

1. **O chip (die de silício)** — tem GPIOs numerados de 0 a 39, com
   buracos: os GPIO 20, 24 e 28–31 não existem em lugar nenhum.
2. **O módulo ESP-WROOM-32** — Contém o
   chip **mais um chip de memória flash** (onde o firmware fica gravado),
   conectados internamente pelos GPIO 6–11.
3. **A placa DevKit** — a PCB que expõe um *subconjunto* dos pinos do
   módulo nos conectores laterais, mais regulador de tensão, chip
   USB-serial e os botões EN/BOOT.

Consequências práticas:

- O **número do GPIO não tem relação com a posição física** no conector.
  A ordem no silkscreen (D13, D12, D14, D27...) é conveniência de
  roteamento da PCB. Nunca se conta pino; lê-se o nome.
- Os **GPIO 6–11 não aparecem** nos conectores da nossa placa de 30
  pinos: a ligação chip↔flash acontece dentro da lata e os projetistas
  não os expuseram — de propósito. (Placas de 38 pinos os expõem como
  CLK, SD0–SD3, CMD; nunca usar.)
- O **GPIO 0 também não está no conector**: na placa de 30 pinos ele é
  ligado apenas ao botão **BOOT**.

## 2. A GPIO matrix: quem decide o que cada pino faz

A ESP32 tem uma **matriz de roteamento** interna: de um lado os
periféricos do chip (I²C, SPI, PWM/LEDC, UART, ADC...), do outro os
pinos físicos (pads), e no meio um "painel de conexões" controlado por
registradores.

Pontos essenciais:

- **Nada é definido antes do firmware rodar.** No reset, os pinos acordam
  num estado neutro (GPIO de entrada). É a inicialização do firmware
  (via drivers do ESP-IDF) que escreve nos registradores e liga, por
  exemplo, o sinal SDA do periférico I²C ao pad 21.
- Cadeia de controle: **código → driver do IDF → registradores →
  matrix → pino**. O mapa de pinos, portanto, vive no código — a cada
  boot ele é reconstruído.
- Quase qualquer função pode ir para quase qualquer pino. Por isso os
  diagramas de pinout mostram várias etiquetas por pino ("GPIO 2 ·
  ADC2_2 · Touch2 · HSPI_CS"): é o **cardápio de funções alternativas**
  daquele pino; o firmware escolhe uma.
- Exceção que importa: alguns sinais têm um caminho direto (**IO_MUX**)
  que não passa pela matrix e permite frequências maiores — caso do
  barramento VSPI nos GPIOs 18/19/23.


## 3. As restrições (os três filtros do mapa)

### 3.1 Proibidos: GPIO 6–11

Ligados fisicamente à flash onde vive o firmware. Usar qualquer um
corrompe o acesso à flash e trava o sistema. Não é convenção — é
impossibilidade. Na nossa placa nem estão expostos.

### 3.2 Strapping: GPIO 0, 2, 12, 15

*Strap* = amarra. No instante do reset, o chip **fotografa o nível
lógico** desses pinos e usa a foto como configuração de partida:

| Pino | O que a "foto" decide |
|---|---|
| GPIO 0 | Boot normal × modo de gravação (é o botão BOOT) |
| GPIO 2 | Participa da decisão de gravação junto com o 0 |
| GPIO 12 | Tensão da flash (nível errado pode danificar em alguns módulos) |
| GPIO 15 | Log de boot habilitado/desabilitado |

Depois do reset viram GPIOs comuns; o risco é o circuito externo
puxá-los para o nível errado *durante* o reset — a placa não liga ou não
grava, e o sintoma parece bug de software. Decisão do projeto: não usar.

### 3.3 Somente entrada: GPIO 34–39

Fisicamente **não têm o estágio de saída** (o par de transistores que
força o pino a 3,3 V ou 0 V) **nem resistores de pull-up/pull-down
internos**. Só observam tensão. São também canais do ADC1 — perfeitos
para leituras analógicas, e só para isso.

### 3.4 ADC1 × ADC2

O ADC2 compartilha hardware com o rádio Wi-Fi: com Wi-Fi ativo, leituras
no ADC2 falham. Decisão do projeto: **todo analógico no ADC1**
(GPIOs 32–39), mesmo no nó que não usa Wi-Fi — o mesmo mapa vale para
qualquer nó e esse conflito nunca precisa ser debugado.

## 4. Conceitos de apoio

**Estágio de saída** — circuito de transistores que permite ao pino
*forçar* um nível (acender LED, gerar clock). Pinos 34–39 não o têm.

**Pull-up** — resistor fraco (~45 kΩ, interno e ativável por software)
ligando o pino a 3,3 V. Resolve o problema da **entrada flutuante**: um
pino de entrada sem nada conectado capta ruído e oscila aleatoriamente.
Exemplo do projeto: o reed-switch liga o GPIO 25 ao GND quando fecha;
com pull-up, chave aberta = 1 estável, chave fechada = 0. Sem pull-up,
chave aberta = leitura aleatória.

**I²C × SPI** — I²C usa 2 fios compartilhados (SDA/SCL) e endereça
dispositivos por software: econômico em pinos, mais lento. SPI usa 3
fios compartilhados (SCK/MISO/MOSI) **+ 1 fio de chip select por
dispositivo**: mais pinos, muito mais rápido.

**NSS** — o chip select do SX1278. *Not Slave Select*: ativo em nível
**baixo** (o rádio só escuta o barramento com NSS em 0 V). No projeto,
**apenas o LoRa usa SPI** — nenhum sensor (BMP280 = I²C, DHT11 =
protocolo próprio de 1 fio, LDR e MH-RD = tensão analógica, reed =
contato seco).

## 5. O mapa e a justificativa pino a pino

| Função | GPIO | Por que este pino |
|---|---|---|
| I²C SDA / SCL (BMP280) | 21 / 22 | Pela matrix iriam em quase qualquer pino; 21/22 são os defaults do datasheet e de todos os exemplos do IDF — convenção de custo zero |
| DHT11 | 4 | Protocolo bidirecional de 1 fio exige entrada E saída (pino ≤ 33); o 4 é livre e não é strapping |
| LDR AO | 34 | Só-entrada + ADC1: usa o pino mais limitado na única coisa que ele faz, poupando pinos completos |
| MH-RD AO | 35 | Mesma lógica do 34 |
| MH-RD DO | 27 | Digital simples, pino comum livre |
| Reed-switch | 25 | Precisa de **pull-up interno** (só existe em pinos ≤ 33) + interrupção (qualquer GPIO tem) |
| LoRa SCK/MISO/MOSI | 18/19/23 | Pinos nativos (IO_MUX) do VSPI: caminho direto sem passar pela matrix, maior velocidade. Para o SX1278 (≤10 MHz) é folga, mas custa zero |
| LoRa NSS | 5 | Pull-up ativo **no boot** + sinal ativo-baixo = LoRa desselecionado enquanto a ESP32 acorda, sem lixo no barramento |
| LoRa RST / DIO0 | 14 / 26 | Escolha livre entre as sobras; DIO0 precisa de interrupção, que todo GPIO tem |
| LED R/G/B | 16/17/13 | LEDC roteia para qualquer saída; eram as sobras. (Em módulos WROVER 16/17 são da PSRAM — não é o nosso caso WROOM-32) |

## 6. Achados na placa real

- Silkscreen usa `D<n>` = GPIO n (D13 = GPIO 13).
- **GPIO 16 e 17 aparecem como `RX2` e `TX2`** (defaults da UART2 —
  nome de função alternativa virando nome de pino). São os pinos do LED
  R e G do nosso mapa: na protoboard, "RX2" = GPIO 16, "TX2" = GPIO 17.
- GPIO 0 ausente do conector; presente só no botão BOOT.
- `RX0`/`TX0` (GPIO 3/1) estão livres no mapa, mas **não usar**: são a
  UART0, o canal do log/monitor serial e da gravação de firmware.
  Pendurar um sensor ali interfere no `make flash`/`make monitor`.
