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

## Próximos passos

1. Ponte SDR → MQTT: sair da impressão em stdout do `crc_verif` para a
   porta de mensagens dele — um bloco Python no próprio
   `receptor_lora.py` assina as PDUs, valida o CRC16 do pacote (o
   fim-a-fim da Fase 3), desserializa os 18 B e publica via paho-mqtt
   no serviço `mosquitto` (rede interna do compose).
2. Definir e documentar o esquema de tópicos.
3. QoS 0; discutir quando 1 faria sentido.
