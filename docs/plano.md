# Plano e próximos passos

## Onde estamos

- **Fases 0–5: concluídas** — ambiente, todos os sensores, firmware
  integrado, enlace LoRa→SDR e MQTT ponta a ponta.
- **Fase 6: concluída na versão simplificada** — dashboard HTML direto
  do broker (WebSocket); stack Grafana registrada como melhoria futura.
- **Fase 7: em aberto** — calibração, CI e revisão final.

## Próximos passos (na ordem prevista)

1. **Fase 7 — CI no GitHub Actions**: buildar os dois firmwares com a
   mesma imagem Docker do desenvolvimento (build reprodutível).
2. **Fase 7 — calibração**: curva do LDR, limiares da chuva, fator
   pulsos→velocidade do vento (documentando limitações).
3. **Extras registrados** (pós-entrega, em ordem de valor):
   SF7/BW125 + teste de perda por `seq`; Telegraf + InfluxDB + Grafana;
   WireViz para o diagrama de fiação; comentar a solução do
   `set_min_output_buffer` no issue #55 do gr-lora_sdr.

## O plano completo (ao vivo, com as caixas reais)

O conteúdo abaixo é o `PLANO.md` da raiz do repositório, incluído
automaticamente no build da documentação — sempre atualizado:

---

--8<-- "PLANO.md"
