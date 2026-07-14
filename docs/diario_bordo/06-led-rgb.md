# 06 — LED RGB de status via LEDC/PWM (Fase 2)

## O que foi feito

- Módulo `led_status.c/.h`: periférico LEDC com 1 timer (5 kHz, 8 bits)
  e 3 canais (R=GPIO16, G=GPIO17, B=GPIO13 — silkscreen da placa chama
  16/17 de RX2/TX2).
- Tabela de estados: BOOT=azul, OK=verde, ERRO_SENSOR=amarelo,
  FALHA=vermelho, APAGADO (cores de LoRa entram na Fase 4).
- Teste isolado no menuconfig: ciclo de cores anunciado no monitor +
  fade do vermelho (duty 0→255→0).

## Conceitos

- **PWM/duty cycle**: liga-desliga rápido (5 kHz); o olho vê a média.
  Duty = fração do período em nível ativo = brilho.
- **LEDC**: timer define frequência/resolução; canais ligam GPIOs ao
  timer com dutys independentes. Cor = trinca de dutys (amarelo =
  vermelho + verde, sem existir LED amarelo).

## Particularidades do hardware (módulo "Wcmcu 3_Clor")

1. **Sem resistores de corrente na placa** (verso liso): teto de duty
   de 25% no software (`LED_DUTY_MAX 64`) para limitar a corrente média.
   Com resistores de 220–330 Ω por cor (lista de compras), subir p/ 255.
2. **Anodo comum com silkscreen mentiroso**: o pino `−` é na verdade o
   comum do ANODO → vai no **3V3**. Descoberto com teste manual (só
   acendeu com comum no 3V3 e cor no GND). No código, resolvido em
   hardware com `flags.output_invert = 1` no canal LEDC — o resto do
   código mantém a semântica "duty = brilho".
3. Ordem dos pinos do módulo: `B R G −` (difere do KY-016 clássico).

## Como validar

Teste "LED RGB" no `make menuconfig` → `make run` → cores acompanham o
monitor (2 s cada) e fade suave do vermelho ao final do ciclo.
