Fase 1 — BMP280 via I²C (temperatura + pressão)

Contexto

Primeiro sensor de verdade do projeto. Objetivo didático: entender o
barramento I²C e leitura de registradores, culminando em temperatura (°C)
e pressão (hPa) plausíveis para Brasília (~26 °C, ~890 hPa) no monitor
serial a cada 5 s. Decisão do usuário: leitura final via componente
pronto (IDF Component Manager), com explicação do que ele faz por dentro.
Os passos intermediários (scanner e leitura do ID) continuam feitos na mão —
são eles que ensinam I²C.

Estado atual

- Fase 0 concluída: make build/run funcionando via compose, hello world
no firmware/estacao/main/estacao.c (contador 1 s).
- .gitignore ignora managed_components/ e dependencies.lock — o
lock deve passar a ser versionado (mesma lógica do package-lock.json:
reprodutibilidade de dependências). Corrigir nesta fase.
- Pinos (CLAUDE.md, não alterar): SDA=GPIO21, SCL=GPIO22. Endereço esperado
0x76 ou 0x77; registrador ID 0xD0 deve retornar 0x58 (0x60 indicaria BME280).

Passos (modo professor: explicação antes de cada código; 1 commit por passo)

1. 📖 Explicação I²C + fiação (sem código): barramento (SDA/SCL,
open-drain, pull-ups — o módulo GY-BMP280 já os tem na placa), endereços
de 7 bits, ACK/NACK, e o driver i2c_master do IDF v5 (bus handle +
device handle). Você liga com USB desconectado: VCC→3V3, GND→GND,
SDA→GPIO21, SCL→GPIO22.
2. Scanner I²C (estacao.c substituindo o hello world): criar o bus com
i2c_new_master_bus e varrer 0x08–0x77 com i2c_master_probe, logando
quem responde. Aceitação: monitor mostra o endereço do BMP280.
3. Ler o registrador de ID na mão: i2c_master_transmit_receive
(escreve 0xD0, lê 1 byte). Aceitação: log chip id = 0x58.
4. Componente pronto: procurar no ESP Component Registry
(components.espressif.com) um driver de BMP280 compatível com IDF v5.x
(verificar na hora — não chutar nome), integrar com
idf.py add-dependency; explicar idf_component.yml,
managed_components/ (regenerável, fora do git) e dependencies.lock
(versionado — remover do .gitignore). Explicar o que o driver faz:
ler os 24 bytes de calibração de fábrica (0x88–0xA1) e aplicar a
compensação do datasheet aos valores brutos.
Fallback se não houver componente bom no registry: esp-idf-lib via git
ou driver mínimo próprio (rediscutir com o usuário).
5. Log periódico: tarefa lendo temperatura e pressão a cada 5 s
(vTaskDelay), log em °C e hPa. Aceitação da fase: valores plausíveis
para Brasília no monitor.
6. Fechamento: docs/01-bmp280.md (o quê, por quê, comandos, problemas,
validação), checkboxes da Fase 1 no PLANO.md, quiz rápido.

Arquivos

- firmware/estacao/main/estacao.c — evolui a cada passo (scanner → ID →
leitura periódica)
- firmware/estacao/main/idf_component.yml — criado pelo add-dependency
- .gitignore — deixar de ignorar dependencies.lock
- docs/01-bmp280.md

Verificação

Cada passo tem saída própria no monitor (make run): endereços do scanner →
chip id = 0x58 → T=26.4 °C P=891.2 hPa a cada 5 s. Build sempre via
make build antes de flashar. Falha de leitura deve logar erro sem travar.

Fora do escopo

Outros sensores (Fase 2), integração/struct (Fase 3), média/filtragem de
amostras.
