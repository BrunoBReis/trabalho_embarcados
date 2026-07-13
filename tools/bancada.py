#!/usr/bin/env python3
"""Teste de bancada: valida o autoteste do firmware pela serial.

Reseta a placa, coleta a serial por N segundos e confere se cada
componente esperado apareceu como "[SELFTEST] <nome> OK". Sai com
codigo 0 se todos OK; 1 caso contrario (falha ou ausente).

Uso (via make test-bancada, dentro do container do IDF):
    python bancada.py --porta /dev/ttyACM0 [--segundos 25]
"""

import argparse
import re
import sys
import time

import serial

ESPERADOS = ["led", "pacote", "lora", "bmp280", "ldr", "chuva", "vento",
             "dht11"]
PADRAO = re.compile(r"\[SELFTEST\] (\w+) (OK|FALHA)")

VERDE = "\033[32m"
VERMELHO = "\033[31m"
AMARELO = "\033[33m"
FIM = "\033[0m"


def main() -> int:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--porta", default="/dev/ttyACM0")
    cli.add_argument("--segundos", type=int, default=25,
                     help="tempo de coleta (o autoteste leva ~8 s)")
    args = cli.parse_args()

    resultados: dict[str, str] = {}

    with serial.Serial(args.porta, 115200, timeout=1) as porta:
        # Reset da placa via DTR/RTS (mesmo mecanismo do esptool).
        porta.setDTR(False)
        porta.setRTS(True)
        time.sleep(0.1)
        porta.setRTS(False)

        fim = time.time() + args.segundos
        while time.time() < fim:
            linha = porta.readline().decode(errors="replace")
            achado = PADRAO.search(linha)
            if achado:
                resultados[achado.group(1)] = achado.group(2)
            # Encerra cedo se todos os esperados ja apareceram.
            if all(nome in resultados for nome in ESPERADOS):
                break

    falhou = False
    print("\n=== Teste de bancada ===")
    for nome in ESPERADOS:
        veredito = resultados.get(nome, "AUSENTE")
        if veredito == "OK":
            cor = VERDE
        elif veredito == "FALHA":
            cor = VERMELHO
            falhou = True
        else:
            cor = AMARELO
            falhou = True
        print(f"  {nome:<8} {cor}{veredito}{FIM}")

    if falhou:
        print(f"\n{VERMELHO}REPROVADO{FIM}: conferir fiacao/monitor serial.")
        return 1
    print(f"\n{VERDE}APROVADO{FIM}: todos os componentes responderam.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
