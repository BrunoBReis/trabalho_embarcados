É exatamente para isso que montamos o menu no Kconfig — o fluxo completo:

1. Escolher o teste:
make menuconfig
Navegue até "Estacao - teste de bancada" → "Modo de operacao no boot" e escolha no botão de rádio:

- estacao (pacote periodico — producao) ← o modo atual
- todos (modo bancada) — autoteste + leitura contínua de tudo
- LDR - luminosidade (GPIO 34, ADC)
- MH-RD - chuva (GPIO 35 AO + GPIO 27 DO)
- DHT11 - umidade/temperatura (GPIO 4)
- Reed-switch - vento (GPIO 25, interrupcao)
- LED RGB - status (GPIOs 16/17/13, LEDC)

Salve com S, saia com Q.

2. Compilar e rodar:
make run     # build + flash + monitor (sair: Ctrl+])

Só isso. O #if CONFIG_... faz o binário conter apenas o teste escolhido — cada um com seu ritmo e formato de log próprios (o do DHT11 lê a cada 2 s, o do vento abre janelas de 5 s, o do LED cicla cores…).

3. Voltar ao normal: make menuconfig de novo → estacao (producao) → make run.

Duas dicas de higiene:

- A escolha fica gravada no sdkconfig, que é versionado — se for commitar algo depois de testar, confere com git diff firmware/estacao/sdkconfig se não está levando a seleção do teste junto (o padrão do repositório é o modo estacao).
- Quando quiser só o veredito rápido de tudo (sem ficar olhando log), o caminho é o modo todos gravado + make test-bancada — o script responde APROVADO/REPROVADO sozinho.

Isso responde? E seguimos com o DHT11: conseguiu replugar o fio no GPIO 4 para fecharmos o teste de recuperação?
