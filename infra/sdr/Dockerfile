# Receptor LoRa via RTL-SDR: GNU Radio 3.10 (apt do Ubuntu 24.04 —
# 3.10.9; o 22.04 traz o 3.10.1, que ignora min_output_buffer e quebra
# o SF12, cf. tapparelj/gr-lora_sdr#55) + gr-lora_sdr compilado da
# fonte, fixado por commit (o projeto nao publica releases).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        git \
        gnuradio-dev \
        gr-osmosdr \
        libspdlog-dev \
        make \
        pkg-config \
        pybind11-dev \
        python3-numpy \
        rtl-sdr \
    && rm -rf /var/lib/apt/lists/*

ARG GR_LORA_SDR_REF=862746dd1cf635c9c8a4bfbaa2c3a0ec3a5306c9

RUN git clone https://github.com/tapparelj/gr-lora_sdr.git /opt/gr-lora_sdr \
    && cd /opt/gr-lora_sdr \
    && git checkout ${GR_LORA_SDR_REF} \
    && cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr \
        -DGR_PYTHON_DIR=/usr/lib/python3/dist-packages \
    && cmake --build build -j"$(nproc)" \
    && cmake --install build \
    && ldconfig

# paho-mqtt instalado DEPOIS da camada pesada de build de proposito:
# adicionar dependencia leve nao invalida o cache da compilacao do
# gr-lora_sdr (~5 min). Se o Dockerfile for reorganizado um dia,
# consolidar no apt-get principal.
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-paho-mqtt \
    && rm -rf /var/lib/apt/lists/*

COPY receptor_lora.py /app/receptor_lora.py
WORKDIR /app

# -u: stdout sem buffer — os pacotes decodificados aparecem na hora no
# `docker compose logs -f`.
CMD ["python3", "-u", "receptor_lora.py"]
