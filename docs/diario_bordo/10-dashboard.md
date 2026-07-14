# 10 — Fase 6: Dashboard mínimo (HTML + MQTT sobre WebSocket)

## Decisão de escopo (12/07/2026)

O foco do trabalho é o embarcado — que já funciona ponta a ponta. A
stack completa (Telegraf + InfluxDB + Grafana) foi adiada como melhoria
futura; o dashboard da entrega é uma página HTML simples. A troca custa
pouco no futuro porque o broker desacopla: o Grafana seria só MAIS UM
assinante do mesmo tópico — nada do que existe muda.

## Arquitetura: o browser é um cliente MQTT

Nenhum backend novo. O Mosquitto ganhou um segundo listener (9001) em
**WebSocket**, e a página usa o mqtt.js (vendorizado no repo — a demo
não depende de internet) para assinar `estacao/v1/dados` direto do
browser. O `retain` da Fase 5 faz a página abrir já mostrando o último
pacote. O nginx (`dashboard` no compose, porta 8080) só serve dois
arquivos estáticos.

```
ponte (container sdr-receptor) --publica--> Mosquitto --roteia--> mosquitto_sub
                                             |  1883              (terminal)
                                             |  9001 (websocket)
                                             +-------> browser (index.html)
```

## O que a página mostra

Cards de temperatura/umidade/pressão/luz/chuva/vento, seq, horário do
pacote e sensores em falha — o card do sensor com flag de erro fica
vermelho (as flags da Fase 3 chegando na interface). Um log dos
últimos 30 pacotes no rodapé.

## Como validar

```bash
cd infra && docker compose up -d       # sobe tudo (broker, receptor*, nginx)
xdg-open http://localhost:8080         # estacao ligada e dongle plugado
```

(*o sdr-receptor precisa do dongle; sem ele, a página ainda abre e
mostra o último pacote retido pelo broker.)

Validado sem browser: assinante paho via `transport='websockets'`
recebeu o pacote retido pelo listener 9001 (seq 58, ao vivo). Página e
mqtt.min.js servidos pelo nginx conferidos por HTTP.

## Melhorias futuras (registradas, não prometidas)

- Telegraf + InfluxDB + Grafana (histórico persistente + gráficos)
- Gráfico simples client-side na própria página (série em memória)
- TLS/senha no broker se sair da bancada
