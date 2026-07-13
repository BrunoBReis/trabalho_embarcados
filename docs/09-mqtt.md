# 09 — Fase 5: MQTT (broker + ponte SDR → MQTT)

## Decisão de priorização (12/07/2026)

SF7 e teste de perda da Fase 4 viraram extras pós-Fase 6: pipeline
completo (MQTT + dashboard) vale mais que enlace otimizado. O enlace
segue em SF12/10 s, funcional.

## Passo 1 — Broker Mosquitto no compose (feito)

MQTT em uma linha: protocolo pub/sub para IoT em que ninguém fala com
ninguém — todos falam com o **broker**, que roteia por **tópicos**
hierárquicos (`estacao/v1/dados`; curinga `estacao/#`). Publicador não
sabe quem assina: o dashboard, um logger e um terminal podem escutar o
mesmo pacote sem a estação saber.

O que foi feito:

- `infra/mosquitto/mosquitto.conf`: o Mosquitto **v2 por padrão só
  aceita conexão local e exige usuário** — para bancada, `listener
  1883` + `allow_anonymous true` explícitos (produto real: senha+TLS).
  `persistence` (retain sobrevive a restart) e log no stdout.
- Serviço no `infra/docker-compose.yml`: imagem **fixada**
  (`eclipse-mosquitto:2.0.20`), porta 1883 exposta no host, config
  montada read-only, volume nomeado `mosquitto-data`,
  `restart: unless-stopped`.

### Validação (feita)

```bash
cd infra && docker compose up -d mosquitto
# dentro do container (os clientes vem na imagem):
docker compose exec mosquitto sh -c \
  "mosquitto_sub -t 'estacao/#' -C 1 -W 10 & sleep 1; \
   mosquitto_pub -t estacao/v1/teste -m 'ola'; wait"
```

Assinante recebeu a mensagem publicada — roteamento ok. Do host (com
`pacman -S mosquitto`): `mosquitto_sub -h localhost -t 'estacao/#' -v`.

## Passo 2 — Ponte SDR → MQTT (feito)

O `receptor_lora.py` ganhou o bloco `PonteMqtt` (mensagens apenas, sem
stream): assina a porta `msg` do `crc_verif`, valida o **CRC16
fim-a-fim** da Fase 3 (réplica exata do `crc16.c`, selfcheck 0x29B1 no
boot), desserializa os 18 B (`struct.unpack('<HhIBHHHBH')` — espelho do
`pacote_t`) e publica JSON no `mosquitto` via paho-mqtt, pela rede
interna do compose (hostname = nome do serviço).

### Decisões documentadas

- **Esquema de tópicos**: JSON único em `estacao/v1/dados` (o `v1`
  espelha a versão do formato de fio). Tópicos por campo ficam para
  quando o dashboard pedir — Telegraf/Node-RED consomem JSON nativamente.
- **QoS 0 + retain**: perda de um pacote de telemetria não justifica
  handshake (o próximo chega em 10 s); QoS 1 faria sentido para
  comandos/alarmes. O retain entrega o último estado a assinante novo.
- **Quem decide validade é o CRC16 da aplicação**, não o CRC do rádio:
  o `crc_verif` publica mesmo com CRC PHY inválido, e a ponte descarta
  (com contador) o que não fechar — pacote corrompido vira estatística.

### A pegadinha do payload binário no PMT

O `crc_verif` publica o payload como *símbolo* PMT (string C++); a
conversão direta para `str` em Python assume UTF-8 e corromperia bytes
altos. Solução: `pmt.serialize_str()` devolve o formato de fio intacto
(`0x02` + tamanho u16 BE + bytes crus — confirmado empiricamente) e a
ponte extrai dali.

### Validação (sem bancada!)

Os dois pacotes reais capturados pelo rádio na Fase 4 foram
reinjetados na ponte como símbolos PMT genuínos (`pmt.deserialize_str`)
dentro do container: CRC16 conferiu, campos decodificados (22,62 °C /
906,1 hPa / 71 %), JSON publicado e lido de volta com `mosquitto_sub`
(que conectou depois — retain comprovado). Caminho de descarte testado
com payload de tamanho errado.

## Aceitação da fase (12/07/2026) ✅

Ao vivo na bancada: `mosquitto_sub -t 'estacao/#' -v` mostrando o JSON
da estação a cada 10 s pelo caminho completo
`sensores → SX1278 → 433 MHz → RTL-SDR → gr-lora_sdr → CRC16 → MQTT`.
A Fase 5 está fechada; próximo: Fase 6 (persistência + dashboard).
