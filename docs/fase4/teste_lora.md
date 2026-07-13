O que é o chirp, em 1 parágrafo

LoRa usa CSS (Chirp Spread Spectrum): cada símbolo é uma "rampa" de frequência que varre a largura de banda inteira (no nosso caso, 125 kHz) de baixo para cima, num tempo fixo. A informação está em onde a rampa começa dentro da banda. Um pacote começa com o preâmbulo — 8 rampas idênticas de ponta a ponta, que é a "assinatura visual" do LoRa — seguido de 2 símbolos de sincronização, e então os símbolos do payload (rampas que começam em pontos diferentes). No waterfall do SDR (gráfico frequência × tempo), isso aparece como linhas diagonais paralelas subindo, dentro de uma faixa de exatamente 125 kHz.

Os números do seu pacote (é isso que você confere)

A duração de cada símbolo é 2^SF / BW. Daí sai tudo:

┌──────────────────────────────────┬────────────────────┬──────────────────────────┐
│           Configuração           │ Duração do símbolo │ Pacote de 18 bytes no ar │
├──────────────────────────────────┼────────────────────┼──────────────────────────┤
│ SF7 / BW 125 kHz (o do enlace)   │ 1,02 ms            │ ~51 ms                   │
├──────────────────────────────────┼────────────────────┼──────────────────────────┤
│ SF12 / BW 125 kHz (o "didático") │ 32,8 ms            │ ~1,3 s                   │
└──────────────────────────────────┴────────────────────┴──────────────────────────┘

Em SF7 as rampas são rápidas demais — no waterfall o pacote vira um "tijolo" brilhante de 125 kHz de largura durando ~50 ms. Por isso o truque para a primeira validação visual: configurar SF12 temporariamente. Cada rampa dura 33 ms e você vê as diagonais do preâmbulo a olho nu, uma a uma. Depois voltamos para SF7.

Como ter certeza de que é o SEU sinal

O critério não é "vi algo em 433 MHz" — essa faixa é o quintal dos controles de portão (concentrados em 433,92 MHz, aparecem como riscos verticais finos e curtos, bem diferentes). A certeza vem de o sinal obedecer aos quatro parâmetros que você controla:

1. Tempo: o clarão aparece no waterfall no instante exato em que o monitor serial loga o TX (vamos transmitir a cada poucos segundos no teste);
2. Frequência central: exatamente onde configuramos (433,0 MHz — de propósito longe dos 433,92 dos controles);
3. Largura: exatamente 125 kHz;
4. A prova definitiva — o sinal te obedece: muda a config para 433,5 MHz e o clarão muda de lugar; muda BW para 250 kHz e ele alarga; muda SF e a duração muda conforme a tabela. Nenhum sinal alheio faz isso.

O que ler (curto e direto)

- Semtech AN1200.22 — "LoRa Modulation Basics": a nota de aplicação clássica, ~26 páginas, explica CSS, SF, BW, CR com figuras de waterfall iguais às que você vai ver. É A referência para essa validação.
- Datasheet do SX1276/77/78/79, capítulo 4.1 ("LoRa Modem"): estrutura do pacote (preâmbulo, sync word, header, CRC) e a fórmula do tempo no ar que usei acima.
- Calculadora de airtime do The Things Network (online): para conferir os ~51 ms sem fazer conta.

Ferramentas no seu Arch

sudo pacman -S gqrx rtl-sdr — o gqrx é o waterfall mais simples de usar. Primeiro obstáculo clássico no Linux: o kernel carrega o driver de TV digital (dvb_usb_rtl28xxu) e sequestra o dongle; o pacote rtl-sdr do Arch já traz as regras udev, mas se o rtl_test reclamar de dispositivo ocupado, é blacklist desse módulo e replugar. Teste de fumaça antes de tudo: rtl_test (detecta o dongle) e depois abrir o gqrx numa rádio FM comercial (~100 MHz) para confirmar que a recepção funciona de ponta a ponta.

Com o caminho do SDR selado como obrigatório, a estratégia de risco fica: validar o TX visual (waterfall) primeiro, e atacar a decodificação (gr-lora_sdr) logo em seguida — se algo emperrar ali, quero descobrir com semanas de antecedência, não na véspera.

Próximo passo concreto quando você estiver com a bancada montada: fiação do Ra-02 no ESP32 pelos jumpers (te passo a tabela pino a pino conferida com o CLAUDE.md — são 8 fios: 3V3, GND, SCK 18, MISO 19, MOSI 23, NSS 5, RST 14, DIO0 26; jumpers curtos e GND firme, SPI começa em 1 MHz por causa da protoboard) e o "hello LoRa": ler o registrador de versão 0x42 esperando 0x12. Uma observação de fiação já que é protoboard + jumper: o pigtail fica conectado no Ra-02 antes de energizar, sempre — inclusive nos testes em que "só vamos ler registrador", porque um TX acidental sem antena é o único erro aqui que não tem undo. Me avisa quando montar que eu preparo o passo.
