# Conceitos — Etapa 02: LDR no ADC

Síntese das discussões de revisão da etapa 02 (diário:
`docs/diario_bordo/02-ldr.md`, código: `firmware/estacao/main/sensor_ldr.c`
e `adc1.c`).

## O sensor: LDR e o módulo KY-018

O **LDR** (Light-Dependent Resistor) é um resistor de sulfeto de cádmio
(CdS), um semicondutor fotocondutivo: no escuro há poucos portadores de
carga livres e a resistência é altíssima (MΩ); com luz, os fótons libertam
elétrons para conduzir e a resistência despenca (kΩ ou menos). **Mais luz =
menos resistência.** A resposta é aproximadamente logarítmica com a
intensidade e lenta (dezenas de ms) — irrelevante para medir claridade do
céu.

### O divisor de tensão

O ADC mede *tensão*, não resistência. O módulo KY-018 resolve isso com um
**divisor de tensão**: LDR em série com um resistor fixo de 10 kΩ (SMD
`103`) entre 3V3 e GND, com o pino `S` no ponto do meio:

```
3V3 ──[ LDR ]──●──[ 10 kΩ ]── GND
               │
               S → GPIO 34
```

`V_S = 3,3 V × 10k / (R_LDR + 10k)`:

- muita luz → R_LDR ~1 kΩ → V_S ≈ 3,0 V → leitura alta;
- escuro → R_LDR ~500 kΩ → V_S ≈ 0,06 V → leitura perto de 0.

O resistor fixo forma o divisor (sem ele não haveria tensão intermediária a
medir) e limita a corrente. Ele também explica o bug do diário: com o LDR
soldado nos furos errados, o caminho 3V3→LDR→S ficou aberto e sobrou o
10 kΩ puxando `S` para GND → **0 cravado** (pino solto daria ruído
flutuante, não zero constante).

### Por que transmitir o valor bruto

O firmware envia `luz_raw` (0–4095) em vez de lux: a conversão exigiria a
curva do LDR específico, que varia peça a peça e não vem calibrada de
fábrica (diferente do BMP280). Decisão registrada no `pacote.h`
("conversao fica p/ calibracao").

## O ADC da ESP32: atenuação e resolução

```c
adc_oneshot_chan_cfg_t chan_cfg = {
    .atten = ADC_ATTEN_DB_12,    // faixa de entrada
    .bitwidth = ADC_BITWIDTH_12, // granularidade
};
```

As duas configurações são **ortogonais**: a atenuação define *qual faixa de
tensão* cabe na régua; o bitwidth define *quantas marcações* a régua tem.

### Atenuação (`.atten`)

O ADC nativo só mede 0 a ~1,1 V (referência interna). Para sinais maiores,
há um **atenuador programável** na entrada — um divisor interno que encolhe
o sinal antes do conversor:

| Atenuação | Fator (≈10^(dB/20)) | Faixa útil aprox. |
|---|---|---|
| 0 dB | ÷1 | até ~0,95 V |
| 2,5 dB | ÷1,33 | até ~1,25 V |
| 6 dB | ÷2 | até ~1,75 V |
| **12 dB** | ÷4 | **até ~3,1 V** |

(dB = medida logarítmica do fator; 12 dB ≈ 4× em tensão. Em IDFs antigos a
opção chamava-se `ADC_ATTEN_DB_11` — mesma coisa renomeada.)

Os 12 dB são forçados pelo circuito: o divisor do KY-018 é alimentado em
3V3, então `S` chega perto de 3,3 V sob luz forte — com 6 dB, tudo acima de
~1,75 V saturaria em 4095 e a metade "clara" da escala se perderia.
Ressalva: mesmo em 12 dB o ADC comprime/satura perto de 3,1–3,2 V; leituras
sob luz muito intensa "grudam" no teto.

### Resolução (`.bitwidth`) — por que 12 bits

12 bits = 2¹² = 4096 níveis (0–4095), ~0,8 mV por contagem. É o **máximo do
hardware** (SAR de 12 bits nativos) e não há motivo para menos:

- menos bits **não reduz ruído** — só joga fora resolução (10 bits ≈ o
  valor de 12 bits sem os 2 bits de baixo);
- o ganho de tempo de conversão é irrelevante lendo a cada segundos;
- o campo do pacote é `uint16_t luz_raw` — 12 bits cabem com folga.

**Resolução ≠ exatidão**: 12 bits diz que a régua tem 4096 marcações, não
que a medida é confiável a 0,8 mV — o ADC da ESP32 tem ruído e
não-linearidades bem maiores que 1 LSB. Captura-se na resolução máxima e a
qualidade se trata por outros meios (média; calibração se preciso).

## Média de 16 amostras

O ADC da ESP32 é ruidoso: com tensão estável, as leituras dançam ±20–50
contagens. A defesa é a **média de N amostras**: ruído aleatório tem média
zero e se cancela na soma, enquanto o sinal se acumula.

O desvio-padrão da média cai com **√N** → com N=16, o ruído efetivo cai 4×
(±40 → ±10). O 16 não é número mágico de datasheet; é o joelho da curva
custo×benefício:

- reduzir mais 2× (total 8×) exigiria **64 amostras** — 4× o trabalho pelo
  mesmo passo (a curva √N pune rápido);
- o custo é desprezível: cada `adc_oneshot_read` leva dezenas de µs; a
  média completa custa <1 ms num loop de 1–30 s;
- 16 é potência de 2 (divisão vira shift) e sem risco de overflow
  (16 × 4095 = 65 520 cabe em `int`);
- ±10 contagens em 4096 sobra para uma grandeza sem calibração absoluta
  ("escuro / nublado / sol").

## Padrão de código: unidade ADC compartilhada

`adc1.c` implementa um singleton preguiçoso: `adc1_obter()` cria a unidade
ADC1 na primeira chamada e devolve o mesmo handle depois. Motivo: o driver
`adc_oneshot` só permite **uma instância por unidade**, e ADC1 é
compartilhado por LDR (canal 6) e chuva (canal 7) — cada sensor configura
seu *canal*, mas a *unidade* é única. (ADC1 sempre; o ADC2 conflita com o
Wi-Fi. GPIOs 34–39 são input-only, ideais para analógico.)
