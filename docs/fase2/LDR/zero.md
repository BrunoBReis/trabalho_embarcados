Por que "0 cravado" aponta para o hardware

- Software errado costuma dar outro sintoma: se eu tivesse configurado o canal errado do ADC, você veria a leitura de um pino flutuante — um número ruidoso dançando (200… 800… 350…), não zero absoluto.
- 0 exato e constante significa que o GPIO 34 está de fato em ~0 V (qualquer coisa abaixo de ~0,1 V lê 0 com atenuação de 12 dB). Ou seja: o pino está fisicamente preso no GND — o que acontece, por exemplo, se o divisor do módulo estiver sem alimentação (aí o resistor interno puxa o S para o GND) ou se S/− estiverem trocados.

O teste que separa software de hardware (30 s)

Com a placa ligada e o monitor aberto, tire o fio do S do GPIO 34 e encoste um jumper do 3V3 direto no GPIO 34:

- Se aparecer ~4095 → ADC + código funcionam; o problema é o módulo/fiação. (E se, com o fio solto de tudo, voltar aquele ruído dançante — confirma de novo: o zero vinha de fora.)
- Se continuar 0 mesmo com 3V3 direto → aí sim seria software (ou pino queimado), e eu investigo o código.

Se confirmar que é o módulo, os suspeitos em ordem

1. Pinagem do KY-018: nesses módulos o pino do meio é o VCC (+), o S fica numa ponta e o −/GND na outra — e o silkscreen varia entre fabricantes! Confere se o que você ligou no 3V3 é mesmo o do meio.
2. VCC sem contato na protoboard (fileira errada é clássico).
3. Módulo realmente ruim (menos provável — LDR é dureza de queimar).

Me diz o resultado do teste do jumper que seguimos dali.
