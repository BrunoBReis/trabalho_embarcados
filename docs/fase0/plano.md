 Fase 0 — Ambiente e hello world

 Contexto

 Primeira fase do PLANO.md: deixar o ciclo build → flash → monitor funcionando
 100% via Docker, com um hello world rodando na ESP32 e o clangd navegando o
 código no LazyVim. É a fundação de todas as fases seguintes.

 Estado atual verificado

 - ✅ Makefile já está na raiz e cobre todos os alvos (help, build, flash, monitor, shell, lsp-setup).
- ✅ Repositório git já iniciado, com commits.
 - ⚠️ O arquivo de config do clangd está salvo como clangd (sem ponto) — o clangd só lê .clangd. Precisa renomear.
 - ⚠️ Usuário não está no grupo uucp (grupos atuais: brunobreis, docker, input, wheel). Detalhe: como flash/monitor rodam via Docker (container roda como root), o flash funcionaria mesmo assim — mas entrar no uucp é o correto para acesso direto no host. Requer sudo + relogin, então você roda o comando.
 - ⚠️ Placa não está conectada agora (sem /dev/ttyACM0). Build não depende dela; flash/monitor ficam para quando plugar.
 - ⚠️ Imagem espressif/idf:release-v5.3 ainda não foi baixada (~2–3 GB no primeiro comando).
 - ❓ prompt.md está untracked — decidir se entra no repo ou no .gitignore (pergunto na hora do commit).

 Passos (cada um termina com algo verificável; modo professor antes de cada código)

 1. Renomear clangd → .clangd (1 comando).
 2. Grupo serial: você roda sudo usermod -aG uucp $USER (via ! comando na sessão ou noutro terminal) e depois valida com a placa plugada: dmesg | tail mostrando CP2102/CH340 e ls -l /dev/ttyACM0 com grupo uucp. Nota: só vale após relogin; não bloqueia os passos seguintes.
 3. Validar Makefile + imagem: make help (host) e make shell (baixa a imagem no primeiro uso; dentro do container, idf.py --version deve mostrar v5.3.x).
   - 📖 Explicação antes: por que fixar a versão da imagem; anatomia do Makefile (?=, .PHONY, alvo padrão, por que montar o repo no mesmo caminho).
 4. Criar o projeto firmware/estacao com idf.py create-project dentro do container, depois make set-target (TARGET=esp32).
   - 📖 Explicação antes: o que o create-project gera (CMakeLists raiz + main/), o que o set-target faz (sdkconfig, toolchain).
 5. Hello world: app_main() com ESP_LOGI + contador + vTaskDelay(pdMS_TO_TICKS(1000)); compilar com make build.
   - 📖 Explicação antes: app_main vs main (o FreeRTOS já está rodando quando seu código começa), o que o build produz (bootloader, tabela de partições, app) e por que vTaskDelay em vez de busy-wait.
 6. LSP: você roda make lsp-setup (usa sudo, ~3 GB em /opt/esp); depois validar no LazyVim: abrir main/estacao.c (pós-build, para existir build/compile_commands.json), testar autocomplete e gd em vTaskDelay. Se stdint.h não resolver, configurar --query-driver conforme CLAUDE.md.
 7. Flash + monitor (quando a placa estiver plugada): make run → ver o contador subindo. Critério de aceitação da fase.
 8. Documentar e commitar: docs/00-ambiente.md (o que foi feito, por quê, comandos, problemas encontrados, como validar) + commits pequenos (um por passo concluído: .clangd+Makefile+PLANO/CLAUDE; projeto criado; hello world; docs).
 Arquivos criados/alterados
 - clangd → .clangd (rename)
 - firmware/estacao/ (CMakeLists.txt, main/estacao.c, sdkconfig gerado)
 - docs/00-ambiente.md
 Verificação
 - make build termina com "Project build complete".
 - make run mostra I (xxxx) estacao: contador: N incrementando a cada 1 s.
 - No LazyVim, gd em vTaskDelay abre o header do FreeRTOS em /opt/esp/idf/....
 Fora do escopo desta fase
 Qualquer sensor (Fase 1+), gateway, LoRa. Quiz rápido ao final para fechar a fase.
