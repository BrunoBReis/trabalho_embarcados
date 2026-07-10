- sdkconfig (linha 395): diz TODOS — porque depois do teste da chuva eu devolvi a escolha ao padrão.
- build/config/sdkconfig.h (linha 300): ainda diz CHUVA — porque esse arquivo é gerado durante o build, e ainda não buildamos desde a troca. No próximo make build, ele será regravado como TODOS.

Então a cadeia, na direção certa, com cada arquivo do nosso repo:

1. main/Kconfig.projbuild      ← EU ESCREVI. Declara: "existe uma escolha
   (o "cardápio")                 chamada teste, com 3 opções, default TODOS".
                                  Não escolhe nada.

2. sdkconfig                   ← A ESCOLHA ATUAL. Editada pelo menuconfig
   (o "pedido")                   (ou pelo meu sed). Linha 395-397 acima:
                                  TODOS=y, os outros comentados.

3. build/config/sdkconfig.h    ← GERADO no build a partir do (2).
   (o "pedido em C")              Vira: #define CONFIG_ESTACAO_TESTE_TODOS 1
                                  (e NENHUM define para LDR/CHUVA).

4. estacao.c                   ← O pré-processador avalia:
                                  #if CONFIG_ESTACAO_TESTE_TODOS   → fica
                                  #if CONFIG_ESTACAO_TESTE_LDR     → APAGADO
                                  #if CONFIG_ESTACAO_TESTE_CHUVA   → APAGADO
                                  Sobra UMA app_main. O compilador nunca
                                  soube que as outras existiram.

O paralelo exato do seu mundo web

Não é o .env — é o process.env.NODE_ENV no webpack. Quando você escreve:

if (process.env.NODE_ENV !== 'production') { /* devtools */ }

o bundler substitui a variável na hora do build e o minificador remove o bloco morto do bundle — código de dev não existe no bundle de produção. O Kconfig é exatamente isso: sdkconfig = as variáveis de build; #if CONFIG_... = o if que o build resolve; binário final = bundle sem o código morto. A diferença é só maturidade: o Kconfig ainda dá o menu, valida dependências entre opções e garante o "só um do grupo" (choice).

O que teríamos que fazer SEM ele (para o papel ficar visível)
