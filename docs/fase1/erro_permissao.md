📖 O que está acontecendo

No host, o device é crw-rw---- root uucp /dev/ttyACM0 — só root e o grupo uucp podem ler/escrever. Você resolveu isso para o seu usuário no host entrando no grupo uucp. Mas dentro do container a história é outra:

- O --device do compose passa o nó do dispositivo para dentro, com as mesmas permissões (dono root, grupo 984 = uucp do host).
- O nosso user: 1000:1000 define UID e GID primário do processo — e nada mais. Grupos suplementares (como o uucp que você ganhou no host) não atravessam a fronteira do container; lá dentro o processo 1000:1000 não pertence a grupo nenhum além do 1000.
- Resultado: o Python do idf.py tenta abrir /dev/ttyACM0, não é root, não é dono, não está no grupo 984 → not readable.

Antes da migração isso não aparecia em containers rodando como root — root ignora permissões. Foi o preço do user: (que continua valendo a pena pelos arquivos do build).

A solução idiomática: group_add

O compose tem um campo exatamente para isso: group_add injeta grupos suplementares no processo do container. Vamos adicionar o GID do dono do device — descoberto dinamicamente pelo Makefile com stat, para não chumbar o 984 (que é específico do Arch; num Debian seria dialout/20):
