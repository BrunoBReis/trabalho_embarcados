📖 Anatomia do Makefile (o que cada construção faz)

- PROJ ?= firmware/estacao — o ?= significa "atribuir só se ainda não tiver valor". Como variáveis passadas na linha de comando (make build PROJ=firmware/gateway) têm precedência, isso cria defaults sobrescrevíveis. É assim que um único Makefile serve os dois nós.
- .DEFAULT_GOAL := help — make sem argumentos mostra a ajuda em vez de executar o primeiro alvo do arquivo (comportamento padrão do Make, que seria surpreendente).
- .PHONY — declara que build, flash etc. são comandos, não arquivos. Sem isso, se um dia existisse um arquivo chamado build na raiz, o Make diria "'build' is up to date" e não faria nada — o Make foi desenhado nos anos 70 para gerar arquivos, e alvos-comando são um uso "torto" que precisa dessa anotação.
- -v "$(CURDIR)":"$(CURDIR)" — monta o repositório no mesmo caminho dentro do container. O motivo é sutil e importante: o build gera build/compile_commands.json com caminhos absolutos (ex.: /home/brunobreis/Projects/.../main/estacao.c). Se o repo fosse montado em /project, esses caminhos não existiriam no host e o clangd ficaria cego.
- Dois "runners": RUN (sem placa) e RUN_DEV (com --device=$(PORT), que passa a serial para dentro do container). A separação garante que make build nunca falhe por a placa estar desconectada — build é puro software.
