# 03 — MH-RD, sensor de chuva (Fase 2)

## O que foi feito

- Módulo `sensor_chuva.c/.h`: AO no GPIO 35 (ADC1_CH7, 12 dB, média de
  16 amostras) + DO no GPIO 27 (entrada digital comum).
- Refatoração junto: módulo `adc1.c/.h` dono da unidade ADC1 (o driver
  `adc_oneshot` só permite criar a unidade uma vez por firmware; LDR e
  chuva agora pedem o handle ao mesmo lugar — padrão singleton).
- Teste isolado no menuconfig: log de AO e DO lado a lado a cada 1 s.

## Conceitos

- **Princípio resistivo**: água fazendo ponte entre as trilhas do pente
  reduz a resistência → tensão do divisor cai → **AO menor = mais água**
  (seco ≈ 4095).
- **AO vs DO**: o AO é a tensão crua; o DO é o veredito de um
  **comparador LM393** — nível baixo quando o AO cruza o limiar definido
  no **trimpot** da plaquinha. Comparador tira decisão do MCU (pode até
  acordar por interrupção) ao custo de virar só sim/não.
- **Calibração em dois níveis**: o trimpot (limiar do DO) ajusta-se na
  bancada; a interpretação do AO (garoa vs chuva forte, curva, deriva por
  corrosão) fica para a Fase 7 — por isso o pacote da Fase 3 transporta o
  `chuva_raw` cru.

## Validação feita na bancada

1. Pente seco: `AO ≈ 4095 | DO: seco`.
2. **Uma gota não basta**: precisa fazer ponte entre trilhas adjacentes —
   dedo bem molhado atravessando trilhas derrubou o AO na hora.
3. LED do DO na plaquinha acendeu com o pente molhado e apagou seco
   (trimpot já estava em posição útil; girar = mover o limiar).

## Notas para o futuro

- Sensores resistivos **corroem** se ficarem energizados molhados
  (eletrólise). Melhoria futura (Fase 3+): alimentar o VCC do módulo por
  um GPIO e energizar só na hora da leitura.

## Como validar

Selecionar o teste "MH-RD - chuva" no `make menuconfig` → `make run` →
molhar o pente (fazendo ponte entre trilhas): AO despenca, DO vira
`MOLHADO`, LED do DO acende; secar reverte tudo.
