# ===== Estação Meteorológica — comandos do projeto =====
# `make` ou `make help` lista os alvos disponíveis.
# A configuração dos containers está declarada em compose.yml;
# overrides por máquina vão em .env (ver .env.example).

-include .env

PROJ        ?= firmware/estacao
PORT        ?= /dev/ttyACM0
IDF_VERSION ?= release-v5.3
TARGET      ?= esp32

# Variáveis que o compose.yml interpola (${...}).
export PORT IDF_VERSION
export REPO_ROOT := $(CURDIR)
export WORK_DIR  := $(CURDIR)/$(PROJ)
export HOST_UID  := $(shell id -u)
# GID do grupo dono da porta serial (uucp no Arch, dialout em Debian):
# o container precisa dele como grupo suplementar para abrir o device.
export PORT_GID  := $(shell stat -c '%g' $(PORT) 2>/dev/null || echo 0)
export HOST_GID  := $(shell id -g)

# Sem terminal (CI, execução por script) o run precisa de -T para não
# tentar alocar um pseudo-TTY.
TTYFLAG = $(shell test -t 0 || echo -T)

COMPOSE = docker compose
# A infra do gateway (broker, receptor SDR, dashboard) tem compose
# proprio; o -f define tambem o diretorio-base dos caminhos relativos.
COMPOSE_INFRA = $(COMPOSE) -f infra/docker-compose.yml

# Documentacao (MkDocs Material em container; pin de versao).
DOCS_IMAGE = squidfunk/mkdocs-material:9.5
WIREVIZ_VERSION = 0.4.1
# Tarefas efêmeras: cada comando cria um container e o destrói no final.
RUN     = $(COMPOSE) run --rm $(TTYFLAG) toolchain
# Variante com a porta serial — apenas flash/monitor/erase precisam da
# placa conectada; o build nunca deve depender dela.
RUN_DEV = $(COMPOSE) run --rm $(TTYFLAG) dev

.PHONY: help set-target build flash erase-flash monitor run menuconfig clean shell lsp-setup test-bancada infra-up infra-down infra-logs infra-build mqtt-sub docs-serve docs-build docs-wireviz zip

help:
	@printf 'Targets disponíveis:\n'
	@printf '  make help           Mostra esta ajuda (alvo padrão)\n'
	@printf '  make set-target     Define o chip alvo do ESP-IDF usando TARGET=%s\n' '$(TARGET)'
	@printf '  make build          Compila o firmware (não exige a placa)\n'
	@printf '  make flash          Grava o firmware usando PORT=%s\n' '$(PORT)'
	@printf '  make erase-flash    Apaga a flash da placa por completo\n'
	@printf '  make monitor        Abre o monitor serial (sair: Ctrl+])\n'
	@printf '  make run            flash + monitor em sequência\n'
	@printf '  make menuconfig     Menu de configuração do projeto\n'
	@printf '  make clean          Limpa o build por completo (fullclean)\n'
	@printf '  make shell          Abre um bash dentro do container\n'
	@printf '  make lsp-setup      Extrai IDF/toolchain para /opt/esp (1x, ~3 GB, p/ clangd)\n'
	@printf '  make test-bancada   Valida o autoteste dos sensores pela serial\n'
	@printf '\nInfra do gateway (broker + receptor SDR + dashboard):\n'
	@printf '  make infra-up       Sobe os serviços em segundo plano\n'
	@printf '  make infra-down     Derruba os serviços\n'
	@printf '  make infra-logs     Segue os logs de todos os serviços (sair: Ctrl+C)\n'
	@printf '  make infra-build    (Re)constrói as imagens da infra\n'
	@printf '  make mqtt-sub       Mostra os dados chegando no broker (sair: Ctrl+C)\n'
	@printf '\nDocumentação (MkDocs Material em container):\n'
	@printf '  make docs-serve     Serve em http://localhost:8000 com live reload\n'
	@printf '  make docs-build     Gera o site estático em site/\n'
	@printf '  make docs-wireviz   Regenera o diagrama de fiação (docs/wireviz/)\n'
	@printf '\nEntrega:\n'
	@printf '  make zip            Zipa os arquivos versionados (commit atual)\n'
	@printf '\nVariáveis (sobrescrever na chamada ou no .env):\n'
	@printf '  PROJ=%s\n' '$(PROJ)'
	@printf '  PORT=%s\n' '$(PORT)'
	@printf '  IDF_VERSION=%s\n' '$(IDF_VERSION)'
	@printf '  TARGET=%s\n' '$(TARGET)'
	@printf '\nExemplos:\n'
	@printf '  make build PROJ=firmware/gateway\n'
	@printf '  make flash PORT=/dev/ttyUSB1\n'

set-target:
	$(RUN) idf.py set-target $(TARGET)

build:
	$(RUN) idf.py build

flash:
	$(RUN_DEV) idf.py -p $(PORT) flash

erase-flash:
	$(RUN_DEV) idf.py -p $(PORT) erase-flash

monitor:
	$(RUN_DEV) idf.py -p $(PORT) monitor

run:
	$(RUN_DEV) idf.py -p $(PORT) flash monitor

menuconfig:
	$(RUN) idf.py menuconfig

clean:
	$(RUN) idf.py fullclean

shell:
	$(RUN) bash

infra-up:
	$(COMPOSE_INFRA) up -d
	@echo "dashboard: http://localhost:8080  |  broker: localhost:1883 (ws 9001)"

infra-down:
	$(COMPOSE_INFRA) down

infra-logs:
	$(COMPOSE_INFRA) logs -f

infra-build:
	$(COMPOSE_INFRA) build

# Assinante de conferência rodando DENTRO do container do broker:
# não exige mosquitto-clients instalado no host.
mqtt-sub:
	$(COMPOSE_INFRA) exec mosquitto mosquitto_sub -t 'estacao/#' -v

docs-serve:
	docker run --rm $(shell test -t 0 && echo -it) -p 8000:8000 \
		-v $(CURDIR):/docs $(DOCS_IMAGE)

docs-build:
	docker run --rm -v $(CURDIR):/docs $(DOCS_IMAGE) build

# Diagrama de fiacao a partir do YAML (docs/wireviz/estacao.yml).
# Raramente roda, entao usa um container descartavel em vez de imagem
# propria; versao do wireviz fixada.
docs-wireviz:
	docker run --rm -v $(CURDIR):/w -w /w python:3.12-slim sh -c "\
		apt-get update -qq >/dev/null && \
		apt-get install -y -qq --no-install-recommends graphviz >/dev/null && \
		pip install -q wireviz==$(WIREVIZ_VERSION) && \
		wireviz docs/wireviz/estacao.yml"
	@echo "gerado: docs/wireviz/estacao.svg (versionar) + png/html/bom"

# Zip de entrega: git archive empacota exatamente o que está versionado
# no commit atual — tudo do .gitignore (build/, site/, .env) fica fora
# de graça. Mudanças não commitadas NÃO entram; o alvo avisa.
ZIP_FILE = trabalho_final-$(shell git rev-parse --short HEAD).zip
zip:
	git archive --format=zip --output=$(ZIP_FILE) HEAD
	@test -z "$$(git status --porcelain)" || \
		echo "atenção: há mudanças não commitadas que ficaram FORA do zip"
	@echo "gerado: $(ZIP_FILE)"

# Requer o firmware gravado com o teste "todos (modo bancada)".
test-bancada:
	$(RUN_DEV) python $(CURDIR)/tools/bancada.py --porta $(PORT)

lsp-setup:
	docker create --name idf-extract espressif/idf:$(IDF_VERSION)
	sudo mkdir -p /opt/esp
	sudo docker cp idf-extract:/opt/esp/idf /opt/esp/
	sudo docker cp idf-extract:/opt/esp/tools /opt/esp/
	docker rm idf-extract
	sudo chown -R $$USER:$$USER /opt/esp
	@echo "OK: /opt/esp/idf e /opt/esp/tools extraídos para o clangd."
