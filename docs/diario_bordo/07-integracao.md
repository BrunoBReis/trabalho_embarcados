# 07 — Firmware integrado da estação (Fase 3)

## O que foi feito

- **Componente compartilhado `common/pacote`** (via `EXTRA_COMPONENT_DIRS`):
  o contrato de fio estação↔gateway. `pacote_t` packed de **18 bytes**,
  CRC16-CCITT implementado à mão com selfcheck no boot (vetor `"123456789"`
  → `0x29B1`, integrado ao `[SELFTEST]`/bancada). O gateway (Fase 4)
  reutiliza o mesmo componente — zero duplicação do formato.
- **Modo estação** (novo default no menuconfig; intervalo configurável,
  `ESTACAO_INTERVALO_S`, default 30 s): lê todos os sensores, monta o
  pacote (ponto fixo, seq, flags, CRC), imprime legível + hex, LED
  refletindo o estado (verde=ok, amarelo=algum sensor em falha).

## O formato (v1) — 18 bytes, little-endian, sem padding

| Offset | Campo | Tipo | Nota |
|---|---|---|---|
| 0 | seq | u16 | detecta pacote perdido |
| 2 | temp_x100 | i16 | °C ×100 (ponto fixo; 2643 = 26,43 °C) |
| 4 | press_pa | u32 | Pa |
| 8 | umid | u8 | % |
| 9 | luz_raw | u16 | ADC 0–4095 (calibração: Fase 7) |
| 11 | chuva_raw | u16 | ADC 0–4095, menor = mais água |
| 13 | vento_ppm | u16 | pulsos do reed/minuto (fator: Fase 7) |
| 15 | flags | u8 | bit0=BMP280 bit1=LDR bit2=CHUVA bit3=DHT11 |
| 16 | crc16 | u16 | CCITT-FALSE dos 16 bytes anteriores |

Por quê assim: JSON de ~150 bytes levaria ~10 s no ar em SF12 (LoRa a
~290 bps); 18 bytes levam ~1,2 s. Ponto fixo evita float no fio; packed
elimina padding do compilador; endianness documentada (nativa do ESP32);
seq+CRC substituem o que o LoRaWAN daria (detecção de perda/corrupção).

## Validação (transcrição real da bancada)

Teste de tolerância a falhas — DHT11 arrancado e replugado **ao vivo**:

```
seq=1: umid=59% flags=0x00   ← saudável
seq=2: umid=0%  flags=0x08   ← fio de dados puxado: bit do DHT11, loop segue
seq=3: umid=0%  flags=0x08   ← seq avança, demais campos normais
seq=4: umid=59% flags=0x00   ← replugado: recuperação sem reboot
```

Hex do seq=1 decodificado (little-endian visível no CRC `b1 a7` = 0xA7B1):
`01 00 | 54 c8 | fb e1 00 00 | 3b | 00 00 | ff 0f | 00 00 | 00 | b1 a7`

Defeitos conhecidos de hardware aparecem fielmente no pacote: BMP280
(temp morta → −142,52 °C) e LDR (solda errada → luz=0). Ambos com
pendência registrada no PLANO; o formato não muda quando forem trocados.

## Lições/observações

- **DHT11 pode travar**: uma leitura interrompida deixou o sensor preso
  (só voltou com power-cycle). Melhoria futura: alimentar sensores por
  GPIO para permitir power-cycle remoto (mesma ideia anti-corrosão do
  MH-RD). No teste transcrito acima, a recuperação foi espontânea.
- `_Static_assert(sizeof == 18)` no header: se alguém mudar a struct e
  esquecer do contrato, o build quebra — não o rádio.

## Pendências

- 📷 Foto da protoboard completa (anexar aqui).

## Como validar

`make run` no modo estação (default): boot mostra `[SELFTEST] ... 0
falha(s)`, e a cada 30 s um pacote legível + 18 bytes em hex com seq
crescendo. Arrancar o fio de dados do DHT11 → próximo pacote com
`flags=0x08` sem travar; `make test-bancada` (modo bancada) → APROVADO.
