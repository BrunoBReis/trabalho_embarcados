Fase 2 — Demais sensores, um por vez

Contexto

Dominar um periférico novo da ESP32 por sensor: ADC (LDR e MH-RD), GPIO
digital (MH-RD DO), protocolo single-wire com timing crítico (DHT11),
interrupção + debounce (reed-switch) e PWM/LEDC (LED RGB). O BMP280 da
Fase 1 fica como está (defeito de hardware documentado; firmware tratado
como funcional). Decisões do usuário: módulos por sensor + menu Kconfig
para escolher o teste, e criação do autoteste + tools/bancada.py
combinados anteriormente.

Arquitetura da fase

- firmware/estacao/main/ ganha um módulo por sensor:
sensor_bmp280.c/.h (extraído do estacao.c atual), sensor_ldr.c/.h,
sensor_chuva.c/.h, sensor_dht11.c/.h, sensor_vento.c/.h,
led_status.c/.h. Cada um expõe init() e ler()/… com structs simples.
- main/Kconfig.projbuild cria menu "Estação — teste de bancada" com uma
choice: teste do LDR / chuva / DHT11 / vento / LED / todos (bancada).
estacao.c vira o dispatcher (#if CONFIG_TESTE_LDR …).
- Modo "todos": roda um autoteste() no boot que loga
[SELFTEST] <sensor> OK|FALHA (detalhe) por item e segue em loop de
leitura contínua.
- tools/bancada.py (pyserial, rodando via service dev do compose +
alvo make test-bancada): reseta a placa, coleta N segundos de serial,
confere os [SELFTEST] ... OK esperados, exit code ≠ 0 se faltar algum.

Passos (📖 explicação antes de cada um; 1 commit por passo; pinos do CLAUDE.md)

1. Refatoração: extrair o BMP280 para sensor_bmp280.c/.h + criar o
Kconfig + dispatcher no estacao.c. Verificável: make menuconfig
mostra o menu novo e o build do modo "todos" continua lendo o BMP280.
2. LDR (GPIO 34, ADC1_CH6) 📖 ADC: resolução 12 bits, atenuação 12 dB,
ruído e média de N amostras (driver esp_adc/adc_oneshot). Teste:
valores 0–4095 mudando ao cobrir/iluminar o LDR. Fiação: S→34.
3. MH-RD (GPIO 35 AO, GPIO 27 DO) 📖 mesma base ADC + leitura digital
com limiar do trimpot. Teste: molhar a plaquinha, comparar AO vs DO.
4. DHT11 (GPIO 4) 📖 single-wire com timing de µs (por que é chato num
RTOS; RMT como solução do IDF). Componente pronto do registry —
verificar candidatos na hora (mesmo método da Fase 1: conferir driver
novo/RMT e IDF v5). Teste: umidade/temperatura plausíveis a cada 2 s.
5. Reed-switch (GPIO 25) 📖 interrupção GPIO vs polling, pull-up
interno, debounce por tempo (ignorar pulsos < 10 ms), contagem por
janela. Teste: aproximar ímã, ver contagem/rpm no log. Fiação: reed
entre GPIO 25 e GND.
6. LED RGB (16/17/13) 📖 LEDC (timer + canal, duty, fade). Tabela de
cores de status: boot=azul, ok=verde, erro sensor=amarelo, falha=vermelho
(LoRa tx entra na Fase 4). Teste: ciclo de cores + cor refletindo o
autoteste.
7. Bancada: autoteste() consolidado + tools/bancada.py +
make test-bancada. Verificável: comando retorna verde com tudo ligado
e vermelho ao desconectar um sensor.
8. Docs e fechamento: docs/02-ldr.md … docs/06-led-rgb.md (um por
sensor, escritos no passo de cada um), checkboxes no PLANO, quiz.

Arquivos

- firmware/estacao/main/: módulos novos + Kconfig.projbuild +
CMakeLists.txt (SRCS) + estacao.c (dispatcher)
- tools/bancada.py, alvo novo no Makefile
- docs/02…06-*.md

Verificação

Cada passo: make build + flash + leitura da serial (pyserial via compose,
como na Fase 1) mostrando valores coerentes do sensor novo; ao final,
make test-bancada valida o conjunto. Hardware necessário por passo é
avisado antes (fiação com USB desconectado).

Fora do escopo

Integração final com struct/pacote (Fase 3), LoRa (Fase 4), calibração
fina (Fase 7).
