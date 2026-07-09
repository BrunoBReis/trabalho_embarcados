# 00 — Ambiente e hello world (Fase 0)

## O que foi feito

Ciclo **build → flash → monitor** 100% dockerizado, com o projeto
`firmware/estacao` criado e um hello world (contador a cada 1 s) compilando.

1. Makefile na raiz orquestrando o `idf.py` dentro da imagem
   `espressif/idf:release-v5.3` (versão fixada = build reprodutível).
2. `.clangd` na raiz (removendo flags do GCC Xtensa que o clangd não conhece).
   Atenção: o nome do arquivo é `.clangd`, **com ponto** — sem o ponto o
   clangd não o encontra.
3. Projeto criado com `idf.py create-project` dentro do container e alvo
   definido com `make set-target` (esp32).
4. `app_main()` com `ESP_LOGI` + `vTaskDelay(pdMS_TO_TICKS(1000))`.
5. `.gitignore` para `build/` e artefatos regeneráveis. O `sdkconfig` fica
   **versionado**: ele é a configuração do projeto — sem ele, outro clone
   compilaria com defaults possivelmente diferentes.

## Por quê (conceitos da fase)

- **Versão fixada da imagem**: `:latest` poderia trocar o IDF por baixo de
  nós e quebrar o build sem nenhuma mudança no código. Com a tag fixa,
  qualquer máquina (inclusive o CI da Fase 7) compila com o mesmo toolchain.
- **`app_main` vs `main`**: não há SO na ESP32. O boot é
  ROM bootloader → bootloader de 2º estágio → tabela de partições → app;
  o startup do IDF sobe o FreeRTOS e o `app_main()` já roda *dentro* de uma
  tarefa. Por isso `vTaskDelay` funciona desde a primeira linha — e por isso
  busy-wait é proibido (segura a CPU e o watchdog reclama).
- **O que o build produz** (e os offsets onde o flash grava):
  `bootloader.bin` (0x1000), `partition-table.bin` (0x8000),
  `estacao.bin` (0x10000). O hello world tem ~159 KB numa partição de 1 MB.

## Ajustes feitos no Makefile durante a fase

- **`-it` → detecção automática de TTY** (`test -t 0`): com `-t` fixo, o
  `docker run` falha quando não há terminal (CI, execuções por script) com
  `the input device is not a TTY`. Interativo continua tendo TTY (necessário
  para o Ctrl+] do monitor).
- **`--user $(id -u):$(id -g)`**: sem isso o container roda como root e todo
  arquivo escrito no volume (`build/`, `sdkconfig`) sai com dono root no
  host — impossível de editar/apagar sem sudo. Com o flag, tudo sai com o
  UID/GID do usuário. O `HOME=/tmp` complementa: o UID 1000 não existe no
  `/etc/passwd` do container, então damos um home gravável para caches.

## Problemas encontrados

- Arquivo de config do clangd estava salvo como `clangd` (sem ponto) —
  invisível para o clangd. Renomeado.
- Com `--user`, o git dentro do container avisa
  `detected dubious ownership in repository at '/opt/esp/idf/components/openthread/openthread'`
  (repositório de dono root lido por não-root). **Inofensivo**: o IDF só usa
  isso para descobrir a versão do componente OpenThread, que não usamos, e o
  build conclui com sucesso. Se um dia atrapalhar, a imagem aceita a variável
  `IDF_GIT_SAFE_DIR` com os caminhos a liberar.

## Comandos usados

```bash
docker pull espressif/idf:release-v5.3                # ~2–3 GB, uma vez
# criação do projeto (uma vez):
docker run --rm -v "$PWD":"$PWD" -w "$PWD/firmware" \
  -u $(id -u):$(id -g) -e HOME=/tmp \
  espressif/idf:release-v5.3 idf.py create-project estacao

make set-target        # define esp32 como alvo (gera sdkconfig e build/)
make build             # compila; termina com "Project build complete"
make run               # flash + monitor (requer a placa em /dev/ttyUSB0)
make lsp-setup         # uma vez, ~3 GB em /opt/esp, para o clangd no host
```

## Acesso à serial (host)

Usuário adicionado ao grupo `uucp` (grupo da serial no Arch):

```bash
sudo usermod -aG uucp $USER   # exige logout/login para valer
```

Observação: como o flash roda via Docker, ele funcionaria mesmo sem isso
(o daemon do Docker tem acesso ao device), mas o grupo é o correto para
acessar `/dev/ttyUSB0` diretamente do host (ex.: picocom).

## Como validar

1. `make build` → termina com `Project build complete`.
2. Placa conectada: `dmesg | tail` mostra o conversor USB-serial
   (CP2102/CH340) e `ls -l /dev/ttyUSB0` mostra grupo `uucp`.
3. `make run` → monitor mostra `I (…) estacao: contador: N` subindo a cada
   1 s (sair com Ctrl+]).
4. LazyVim: abrir `firmware/estacao/main/estacao.c` (após um build),
   autocomplete funciona e `gd` em `vTaskDelay` abre o header do FreeRTOS em
   `/opt/esp/idf/...`. Se headers da libc não resolverem, usar
   `--query-driver=/opt/esp/tools/**/xtensa-esp32-elf-gcc` (ver CLAUDE.md).
