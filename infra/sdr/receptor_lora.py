#!/usr/bin/env python3
"""Receptor LoRa + ponte MQTT: RTL-SDR -> gr-lora_sdr -> Mosquitto.

Cadeia identica ao exemplo oficial (examples/lora_RX.py), trocando o
USRP pelo RTL-SDR via osmosdr, mais um bloco final que assina a porta
de mensagens do crc_verif, valida o CRC16 fim-a-fim do pacote (formato
em common/pacote/include/pacote.h), desserializa os 18 bytes e publica
JSON no broker. Os parametros do modem PRECISAM bater com o firmware
(firmware/estacao/main/lora.c): 433,0 MHz, BW 125 kHz, CR 4/5, sync
word 0x12, CRC on, header explicito. SF por variavel de ambiente.
"""

import json
import os
import signal
import struct
import sys
import time

import gnuradio.lora_sdr as lora_sdr
import osmosdr
import pmt
from gnuradio import blocks, gr

import paho.mqtt.client as mqtt

FREQ = int(float(os.environ.get("LORA_FREQ", 433e6)))
BW = int(os.environ.get("LORA_BW", 125000))
SF = int(os.environ.get("LORA_SF", 12))
CR = int(os.environ.get("LORA_CR", 1))  # 1 = 4/5
# 250 kS/s: dentro da faixa baixa valida do RTL2832U (225-300 kS/s) e
# multiplo inteiro do BW (os_factor = 2), exigencia do frame_sync.
TAXA = int(os.environ.get("SDR_RATE", 250000))
GANHO = float(os.environ.get("SDR_GAIN", 30))
PPM = int(os.environ.get("SDR_PPM", 0))  # v3 tem TCXO; 0 e o esperado

MQTT_HOST = os.environ.get("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.environ.get("MQTT_PORT", 1883))
TOPICO_DADOS = os.environ.get("MQTT_TOPICO_DADOS", "estacao/v1/dados")

SYNC_WORD = 0x12  # default "rede privada" do SX1278, igual ao firmware
PREAMBULO = 8

# LDRO nao viaja no header LoRa: as duas pontas combinam por fora. O
# firmware liga quando o simbolo passa de 16 ms (datasheet); espelhamos
# o criterio para acompanhar automaticamente a troca SF12 -> SF7.
LDRO = 1 if (2**SF) / BW > 16e-3 else 0

# ---------------------------------------------------------------
# Pacote da estacao (contrato de fio da Fase 3 — pacote.h)
# ---------------------------------------------------------------

# little-endian, sem padding: seq u16 | temp i16 | press u32 | umid u8 |
# luz u16 | chuva u16 | vento u16 | flags u8 | crc16 u16  = 18 bytes
FORMATO_PACOTE = "<HhIBHHHBH"
TAM_PACOTE = struct.calcsize(FORMATO_PACOTE)
assert TAM_PACOTE == 18, "formato do pacote nao bate com pacote.h"

FLAGS_NOMES = {0: "bmp280", 1: "ldr", 2: "chuva", 3: "dht11"}


def crc16_ccitt(dados: bytes) -> int:
    """Mesmo CRC16-CCITT-FALSE de common/pacote/crc16.c (poly 0x1021,
    init 0xFFFF). Selfcheck: crc16_ccitt(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for b in dados:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def desserializar(payload: bytes) -> dict:
    """18 bytes -> dict com unidades fisicas. Levanta ValueError se o
    tamanho ou o CRC16 fim-a-fim nao conferirem."""
    if len(payload) != TAM_PACOTE:
        raise ValueError(f"tamanho {len(payload)} != {TAM_PACOTE}")
    (seq, temp_x100, press_pa, umid, luz, chuva, vento, flags,
     crc_rx) = struct.unpack(FORMATO_PACOTE, payload)
    crc_calc = crc16_ccitt(payload[:-2])
    if crc_rx != crc_calc:
        raise ValueError(f"crc16 0x{crc_rx:04X} != calculado 0x{crc_calc:04X}")
    return {
        "seq": seq,
        "temp_c": round(temp_x100 / 100.0, 2),
        "press_hpa": round(press_pa / 100.0, 1),
        "umid_pct": umid,
        "luz_raw": luz,
        "chuva_raw": chuva,
        "vento_ppm": vento,
        "flags": flags,
        "sensores_em_falha": [n for b, n in FLAGS_NOMES.items()
                              if flags & (1 << b)],
        "ts": int(time.time()),
    }


def payload_de_pmt(msg) -> bytes:
    """Extrai os bytes crus de um simbolo PMT.

    O crc_verif publica o payload como SIMBOLO (string C++). A conversao
    direta p/ str em Python assume UTF-8 e corrompe/explode com payload
    binario; pmt.serialize_str devolve o formato de fio do PMT intacto:
    opcode do tipo (1 B) + tamanho u16 big-endian + bytes crus.
    """
    bruto = bytes(pmt.serialize_str(msg))
    if len(bruto) < 3:
        raise ValueError(f"pmt serializado curto demais: {bruto.hex()}")
    tamanho = struct.unpack(">H", bruto[1:3])[0]
    dados = bruto[3:3 + tamanho]
    if len(dados) != tamanho:
        raise ValueError("pmt serializado truncado")
    return dados


class PonteMqtt(gr.basic_block):
    """Bloco so-de-mensagens: crc_verif 'msg' -> JSON no Mosquitto."""

    def __init__(self, cliente_mqtt):
        gr.basic_block.__init__(self, name="ponte_mqtt",
                                in_sig=None, out_sig=None)
        self.cliente = cliente_mqtt
        self.validos = 0
        self.invalidos = 0
        self.message_port_register_in(pmt.intern("in"))
        self.set_msg_handler(pmt.intern("in"), self.tratar)

    def tratar(self, msg):
        try:
            payload = payload_de_pmt(msg)
            dados = desserializar(payload)
        except ValueError as e:
            self.invalidos += 1
            print(f"[ponte] pacote descartado ({e}) — "
                  f"{self.invalidos} invalido(s) ate agora", flush=True)
            return
        self.validos += 1
        # QoS 0 (fire and forget) + retain: assinante novo recebe o
        # ultimo estado na hora, sem esperar o proximo pacote.
        self.cliente.publish(TOPICO_DADOS, json.dumps(dados),
                             qos=0, retain=True)
        print(f"[ponte] seq={dados['seq']} T={dados['temp_c']}C "
              f"P={dados['press_hpa']}hPa U={dados['umid_pct']}% "
              f"-> {TOPICO_DADOS} (total {self.validos})", flush=True)


def criar_cliente_mqtt():
    try:  # paho 2.x exige a versao da API de callbacks; 1.x nao tem
        cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    except AttributeError:
        cliente = mqtt.Client()
    cliente.connect_async(MQTT_HOST, MQTT_PORT, keepalive=30)
    cliente.loop_start()  # thread propria: reconecta sozinho
    return cliente


class ReceptorLora(gr.top_block):
    def __init__(self, cliente_mqtt):
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
        # print 2 = hex no stdout (conferencia lado a lado com o log da
        # estacao); a ponte e quem valida de verdade, via CRC16 proprio.
        self.crc = lora_sdr.crc_verif(2, False)
        self.ponte = PonteMqtt(cliente_mqtt)

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
        self.msg_connect((self.crc, "msg"), (self.ponte, "in"))
        self.connect(self.fonte, self.copia, self.frame_sync, self.fft_demod,
                     self.gray, self.deinter, self.hamming, self.header,
                     self.dewhite, self.crc)


def main():
    assert crc16_ccitt(b"123456789") == 0x29B1, "selfcheck do CRC16 falhou"

    cliente = criar_cliente_mqtt()
    tb = ReceptorLora(cliente)

    def parar(_sig, _frame):
        tb.stop()
        tb.wait()
        cliente.loop_stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, parar)
    signal.signal(signal.SIGTERM, parar)

    print(f"[receptor] {FREQ/1e6:.3f} MHz | SF{SF} | BW {BW//1000} kHz | "
          f"CR 4/{4+CR} | LDRO {'on' if LDRO else 'off'} | "
          f"{TAXA/1e3:.0f} kS/s (os={TAXA//BW}) | ganho {GANHO} dB | "
          f"mqtt {MQTT_HOST}:{MQTT_PORT} -> {TOPICO_DADOS}",
          flush=True)
    tb.run()


if __name__ == "__main__":
    main()
