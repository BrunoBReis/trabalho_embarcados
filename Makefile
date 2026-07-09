# ===== Estação Meteorológica — comandos do projeto =====
# `make` ou `make help` lista os alvos disponíveis.

PROJ   ?= firmware/estacao
PORT   ?= /dev/ttyUSB0
IMAGE  ?= espressif/idf:release-v5.3
TARGET ?= esp32

.DEFAULT_GOAL := help

# -t (TTY) só quando há um terminal de verdade — em CI ou execução
# não interativa o docker falharia com "the input device is not a TTY".
TTY = $(shell test -t 0 && echo -it || echo -i)

# --user com o UID/GID do host: sem isso o container roda como root e
# todo arquivo gerado no volume (build/, sdkconfig) sai com dono root.
ASUSER = --user $(shell id -u):$(shell id -g)

# O repositório é montado NO MESMO caminho do host para que os caminhos
# do compile_commands.json gerado no container sejam válidos no host.
RUN = docker run --rm $(TTY) $(ASUSER) \
	-v "$(CURDIR)":"$(CURDIR)" \
	-w "$(CURDIR)/$(PROJ)" \
	-e HOME=/tmp \
	$(IMAGE)

# Variante com acesso à porta serial — apenas flash/monitor precisam
# da placa conectada; o build nunca deve depender dela.
RUN_DEV = docker run --rm $(TTY) $(ASUSER) \
	-v "$(CURDIR)":"$(CURDIR)" \
	-w "$(CURDIR)/$(PROJ)" \
	--device=$(PORT) \
	-e HOME=/tmp \
	$(IMAGE)

.PHONY: help set-target build flash monitor run menuconfig clean shell lsp-setup

help:
	@printf 'Targets disponíveis:\n'
	@printf '  make help           Mostra esta ajuda (alvo padrão)\n'
	@printf '  make set-target     Define o chip alvo do ESP-IDF usando TARGET=%s\n' '$(TARGET)'
	@printf '  make build          Compila o firmware (não exige a placa)\n'
	@printf '  make flash          Grava o firmware usando PORT=%s\n' '$(PORT)'
	@printf '  make monitor        Abre o monitor serial (sair: Ctrl+])\n'
	@printf '  make run            flash + monitor em sequência\n'
	@printf '  make menuconfig     Menu de configuração do projeto\n'
	@printf '  make clean          Limpa o build por completo (fullclean)\n'
	@printf '  make shell          Abre um bash dentro do container\n'
	@printf '  make lsp-setup      Extrai IDF/toolchain para /opt/esp (1x, ~3 GB, p/ clangd)\n'
	@printf '\nVariáveis (sobrescrever na chamada, ex.: make build PROJ=firmware/gateway):\n'
	@printf '  PROJ=%s\n' '$(PROJ)'
	@printf '  PORT=%s\n' '$(PORT)'
	@printf '  IMAGE=%s\n' '$(IMAGE)'
	@printf '  TARGET=%s\n' '$(TARGET)'

set-target:
	$(RUN) idf.py set-target $(TARGET)

build:
	$(RUN) idf.py build

flash:
	$(RUN_DEV) idf.py -p $(PORT) flash

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

lsp-setup:
	docker create --name idf-extract $(IMAGE)
	sudo mkdir -p /opt/esp
	sudo docker cp idf-extract:/opt/esp/idf /opt/esp/
	sudo docker cp idf-extract:/opt/esp/tools /opt/esp/
	docker rm idf-extract
	sudo chown -R $$USER:$$USER /opt/esp
	@echo "OK: /opt/esp/idf e /opt/esp/tools extraídos para o clangd."
