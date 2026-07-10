Fase 3 — Firmware integrado da estação

Contexto

Transformar os módulos da Fase 2 numa estação: a cada 30 s, ler todos
os sensores, preencher o pacote binário de ~18 bytes (struct packed,
ponto fixo, CRC16) definido em common/ — formato que irá pelo LoRa na
Fase 4 — imprimindo legível E em hex no monitor. Falha de sensor vira bit
de flag (sistema nunca trava); LED RGB reflete o estado real. Marco:
"dados no formato que eu quero".

A montagem física já está completa (bancada da Fase 2 aprovou os 6
componentes juntos); falta a foto para o docs/07 (usuário).

Arquitetura

- common/pacote/ vira um componente IDF compartilhado (a lição
de organização da fase): include/pacote.h (struct + flags + API),
pacote.c (montar/serializar/validar), crc16.c/.h (CRC16-CCITT
implementado à mão — é objetivo de aprendizado). O projeto da estação
o enxerga via EXTRA_COMPONENT_DIRS no CMakeLists raiz — e o gateway
(Fase 4) reutiliza o mesmo componente sem copiar código.
- Struct (__attribute__((packed)), little-endian nativo do ESP32,
documentado no header):
seq u16 | temp_x100 i16 | press_pa u32 | umid u8 | luz_raw u16 | chuva_raw u16 | vento_ppm u16 | flags u8 | crc16 u16 = 18 bytes.
Flags: 1 bit de erro por sensor (BMP280, LDR, CHUVA, DHT11) + reservas.
- Novo modo no Kconfig: opção "estacao (pacote periodico)" no choice
existente (vira o default; o modo bancada continua disponível) + um
int de intervalo (ESTACAO_INTERVALO_S, default 30) — de quebra,
aprende-se config numérica no Kconfig.

Passos (📖 explicação antes; 1 commit por passo)

1. 📖 + componente common/pacote: struct packed (por que packed;
endianness; ponto fixo temp_x100), CRC16-CCITT à mão com
auto-verificação no boot (vetor conhecido: CRC de "123456789" =
0x29B1) — se a implementação regredir, o firmware acusa no boot.
Verificável: make build com o componente linkado + selfcheck OK.
2. Modo estação: Kconfig (nova opção + intervalo), tarefa principal:
ler todos os sensores reutilizando os módulos (sensor_*_ler,
sensor_vento_coletar_pulsos → pulsos×60/intervalo = vento_ppm),
falha → campo 0 + bit na flag, seq++, CRC, log legível + hex dump
do pacote, LED OK/ERRO_SENSOR conforme flags. Verificável: pacote no
monitor a cada intervalo.
3. Teste de tolerância a falhas (aceitação da fase): desconectar o
DHT11 com o sistema rodando → flag correspondente aparece, campo zera,
loop segue, LED amarelo; reconectar → volta ao normal. Regressão:
make test-bancada continua APROVADO (modo bancada re-selecionável).
4. Fechamento: docs/07-integracao.md (formato do pacote byte a
byte, decisões, validação; foto da protoboard = pendência do usuário),
checkboxes no PLANO, quiz.

Arquivos

- common/pacote/: CMakeLists.txt, include/pacote.h, pacote.c, crc16.c/.h
- firmware/estacao/CMakeLists.txt: EXTRA_COMPONENT_DIRS
- firmware/estacao/main/: Kconfig.projbuild (opção nova + intervalo),
estacao.c (modo estação), CMakeLists (nada novo além do include do
componente)
- docs/07-integracao.md

Verificação

1. Boot: [pacote] selfcheck CRC16 OK.
2. A cada 30 s: linha legível + hex de 18 bytes; seq incrementando.
3. Desconectar DHT11 ao vivo: flags=0x08 (bit do DHT11), sistema segue,
LED amarelo; reconectar normaliza.
4. make test-bancada (com modo bancada selecionado) segue APROVADO.

Fora do escopo

LoRa/SPI (Fase 4), conversão física de luz/chuva/vento (Fase 7),
economia de energia.
