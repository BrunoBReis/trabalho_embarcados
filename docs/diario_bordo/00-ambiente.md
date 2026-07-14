# 00 — Ambiente e hello world (Fase 0)

## O que foi feito

Ciclo **build → flash → monitor** 100% dockerizado, com o projeto
`firmware/estacao` criado e um hello world (contador a cada 1 s) compilando.

1. Makefile na raiz orquestrando o `idf.py` dentro da imagem
   `espressif/idf:release-v5.3` (versão fixada = build reprodutível).
2. `.clangd` na raiz (removendo flags do GCC Xtensa que o clangd não conhece).
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

## Decisão: compose run --rm (tarefa efêmera declarada em YAML)

Depois do primeiro build funcionando com `docker run` puro, a gestão do
toolchain migrou para **`docker compose run --rm`** (`compose.yml` na raiz):

- **Por que não `compose up` + `exec`** (padrão devcontainer): compose
  orquestra *serviços* (processos duradouros, portas, rede entre eles);
  o compilador é uma *tarefa* — roda e termina. Container de longa duração
  reintroduz estado (drift), falha no `up` sem a placa plugada e exige
  `down`/`up` a cada replug da ESP32 (Docker não faz hot-plug de device).
- **Por que não ficar no `docker run` puro**: a configuração (volumes,
  device, user) ficava implícita em flags dentro do Makefile. O
  `compose.yml` declara isso de forma legível e versionada.
- **`compose run --rm`** dá os dois: declaração em YAML + container
  efêmero por comando. Dois services: `toolchain` (sem device — build)
  e `dev` (com a serial — flash/monitor), unidos por âncora YAML.
- `.env` (fora do git; ver `.env.example`) permite override por máquina
  (`PORT`, `IDF_VERSION`) sem tocar em arquivo versionado.
- A infra de *serviços de verdade* (Mosquitto e Dashboard) terá o
  próprio compose em `infra/` nas Fases 5–6 — lá com `up -d`, restart
  policy e volumes.

## Ajustes feitos no Makefile durante a fase

- **Detecção automática de TTY** (`test -t 0`): sem terminal (CI, execução
  por script) o `compose run` precisa de `-T` para não tentar alocar um
  pseudo-TTY. Interativo continua tendo TTY (necessário para o Ctrl+] do
  monitor e para o menuconfig).
- **`user:` com UID/GID do host** (no compose.yml): sem isso o container
  roda como root e todo arquivo escrito no volume (`build/`, `sdkconfig`)
  sai com dono root no host — impossível de editar/apagar sem sudo. O
  `HOME=/tmp` complementa: o UID 1000 não existe no `/etc/passwd` do
  container, então damos um home gravável para caches.
- **Alvo `erase-flash`**: apaga a flash inteira quando ela fica num estado
  estranho (NVS corrompida etc.).

## Comandos usados

```bash
docker pull espressif/idf:release-v5.3                # ~2–3 GB, uma vez
# criação do projeto (uma vez):
docker run --rm -v "$PWD":"$PWD" -w "$PWD/firmware" \
  -u $(id -u):$(id -g) -e HOME=/tmp \
  espressif/idf:release-v5.3 idf.py create-project estacao

make set-target        # define esp32 como alvo (gera sdkconfig e build/)
make build             # compila; termina com "Project build complete"
make run               # flash + monitor (requer a placa em /dev/ttyACM0)
make lsp-setup         # uma vez, ~3 GB em /opt/esp, para o clangd no host
```

## Acesso à serial (host)

Usuário adicionado ao grupo `uucp` (grupo da serial no Arch):

```bash
sudo usermod -aG uucp $USER   # exige logout/login para valer
```

Observação: como o flash roda via Docker, ele funcionaria mesmo sem isso
(o daemon do Docker tem acesso ao device), mas o grupo é o correto para
acessar `/dev/ttyACM0` diretamente do host (ex.: picocom).
