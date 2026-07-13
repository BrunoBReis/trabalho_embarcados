#!/usr/bin/env python3
"""Receptor LoRa: RTL-SDR -> gr-lora_sdr -> payload no stdout.

Cadeia identica ao exemplo oficial (examples/lora_RX.py), trocando o
USRP pelo RTL-SDR via osmosdr. Os parametros do modem PRECISAM bater
com o firmware da estacao (firmware/estacao/main/lora.c):
433,0 MHz, BW 125 kHz, CR 4/5, sync word 0x12, CRC ligado, header
explicito. SF e o unico que muda com frequencia (12 didatico / 7
producao), por isso vem de variavel de ambiente.
"""

import os
import signal
import sys

import gnuradio.lora_sdr as lora_sdr
import osmosdr
from gnuradio import blocks, gr

FREQ = int(float(os.environ.get("LORA_FREQ", 433e6)))
BW = int(os.environ.get("LORA_BW", 125000))
SF = int(os.environ.get("LORA_SF", 12))
CR = int(os.environ.get("LORA_CR", 1))  # 1 = 4/5
# 250 kS/s: dentro da faixa baixa valida do RTL2832U (225-300 kS/s) e
# multiplo inteiro do BW (os_factor = 2), exigencia do frame_sync.
TAXA = int(os.environ.get("SDR_RATE", 250000))
GANHO = float(os.environ.get("SDR_GAIN", 30))
PPM = int(os.environ.get("SDR_PPM", 0))  # v3 tem TCXO; 0 e o esperado
FORMATO = os.environ.get("LORA_FORMATO", "ascii")  # ascii | hex

SYNC_WORD = 0x12  # default "rede privada" do SX1278, igual ao firmware
PREAMBULO = 8

# LDRO nao viaja no header LoRa: as duas pontas combinam por fora. O
# firmware liga quando o simbolo passa de 16 ms (datasheet); espelhamos
# o criterio para acompanhar automaticamente a troca SF12 -> SF7.
LDRO = 1 if (2**SF) / BW > 16e-3 else 0


class ReceptorLora(gr.top_block):
    def __init__(self):
        gr.top_block.__init__(self, "receptor lora (rtl-sdr)")

        if TAXA % BW != 0:
            raise SystemExit(f"SDR_RATE ({TAXA}) deve ser multiplo do BW ({BW})")
        os_factor = TAXA // BW

        self.fonte = osmosdr.source(args="numchan=1 rtl=0")
        self.fonte.set_sample_rate(TAXA)
        self.fonte.set_center_freq(FREQ, 0)
        self.fonte.set_freq_corr(PPM, 0)
        self.fonte.set_gain_mode(False, 0)  # ganho manual, como no gqrx
        self.fonte.set_gain(GANHO, 0)

        # A cadeia do PHY LoRa, na ordem do exemplo oficial:
        # sincronizacao -> FFT (chirp -> simbolo) -> de-Gray ->
        # desentrelacamento -> Hamming -> header -> dewhitening -> CRC.
        soft = True
        impl_head = False  # header explicito, como no firmware
        self.frame_sync = lora_sdr.frame_sync(FREQ, BW, SF, impl_head,
                                              [SYNC_WORD], os_factor,
                                              PREAMBULO)
        self.fft_demod = lora_sdr.fft_demod(soft, True)
        self.gray = lora_sdr.gray_mapping(soft)
        self.deinter = lora_sdr.deinterleaver(soft)
        self.hamming = lora_sdr.hamming_dec(soft)
        # pay_len=255 e ignorado com header explicito (vem do proprio
        # header); print_header=True mostra len/CR/CRC de cada quadro.
        self.header = lora_sdr.header_decoder(impl_head, CR, 255, True,
                                              LDRO, True)
        self.dewhite = lora_sdr.dewhitening()
        self.crc = lora_sdr.crc_verif(2 if FORMATO == "hex" else 1, False)

        # O frame_sync precisa de um simbolo inteiro por chamada
        # (2^SF * os_factor amostras; 8196 em SF12/os=2), acima do
        # buffer padrao do GNU Radio (8191). Este bloco de copia so
        # existe para carregar um buffer maior (4 simbolos) na entrada.
        # ATENCAO: usar a forma (porta, tamanho) — a variante de 1
        # argumento e ignorada pelo binding python do GR 3.10 (testado
        # empiricamente; e a causa do erro do issue #55 do gr-lora_sdr).
        self.copia = blocks.copy(gr.sizeof_gr_complex)
        self.copia.set_min_output_buffer(0, int((2**SF) * os_factor * 4))

        self.msg_connect((self.header, "frame_info"),
                         (self.frame_sync, "frame_info"))
        self.connect(self.fonte, self.copia, self.frame_sync, self.fft_demod,
                     self.gray, self.deinter, self.hamming, self.header,
                     self.dewhite, self.crc)


def main():
    tb = ReceptorLora()

    def parar(_sig, _frame):
        tb.stop()
        tb.wait()
        sys.exit(0)

    signal.signal(signal.SIGINT, parar)
    signal.signal(signal.SIGTERM, parar)

    print(f"[receptor] {FREQ/1e6:.3f} MHz | SF{SF} | BW {BW//1000} kHz | "
          f"CR 4/{4+CR} | LDRO {'on' if LDRO else 'off'} | "
          f"{TAXA/1e3:.0f} kS/s (os={TAXA//BW}) | ganho {GANHO} dB",
          flush=True)
    tb.run()


if __name__ == "__main__":
    main()
