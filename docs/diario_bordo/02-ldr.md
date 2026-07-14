# 02 — LDR no ADC (Fase 2)

## O que foi feito

- Módulo `sensor_ldr.c/.h`: ADC1 canal 6 (GPIO 34, input-only), driver
  `adc_oneshot` do IDF (nenhuma dependência externa — ADC é periférico do
  próprio ESP32), atenuação 12 dB, 12 bits (0–4095), **média de 16
  amostras** contra o ruído do ADC.
- Teste isolado selecionável no `menuconfig` (Estacao - teste de bancada):
  log do valor bruto a cada 1 s.

## Conceitos

- **ADC**: tensão → número. 12 bits = 0–4095; com 12 dB a faixa útil vai
  até ~3,1 V. ADC1 sempre (ADC2 conflita com o Wi-Fi); GPIOs 34–39 são
  input-only, ideais para analógico.
- **LDR em divisor de tensão**: o módulo põe o LDR em série com um
  resistor fixo (SMD `103` = 10 kΩ); a luz muda a resistência do LDR e a
  tensão do ponto do meio (S) acompanha.
- **Ruído e média**: o ADC da ESP32 dança ±20–50 contagens; média de N
  amostras cancela ruído aleatório.

## 🐛 Diagnóstico: módulo KY-018 com LDR soldado nos furos errados

Sintoma: leitura **0 cravado** em qualquer condição de luz.

1. `0` constante ≠ pino flutuante (que daria ruído variando) → o pino
   estava de fato em ~0 V.
2. **Teste do jumper**: 3V3 direto no GPIO 34 → leu **4095** → ADC e
   software absolvidos.
3. Inspeção visual comparando com um KY-018 de referência: o LDR está
   soldado nos **furos da borda (não conectados)**; os furos corretos
   (marcação `S1`, centro da placa) estão vazios com resto de solda e
   marca de retrabalho. Caminho 3V3→LDR→S aberto → o resistor de 10 kΩ
   puxa o S para GND → 0 permanente.

**Pendência**: consertar (ressoldar o LDR nos furos do `S1`) ou montar o
divisor na protoboard com LDR avulso + resistor ~10 kΩ:
`3V3 —[LDR]—●—[10k]—GND`, nó central no GPIO 34.

## Como validar (quando o hardware existir)

1. Selecionar o teste do LDR no `make menuconfig` → `make run`.
2. `luz (bruto 0-4095): N` a cada 1 s; cobrir o LDR e iluminar com
   lanterna deve mover o valor de forma clara e estável.
