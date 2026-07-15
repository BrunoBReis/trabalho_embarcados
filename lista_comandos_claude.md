- `groups; echo ---; docker --version 2>&1; echo ---; ls /dev/ttyUSB* 2>&1; echo ---; docker images espressif/idf 2>&1 | head -5`
- `docker pull espressif/idf:release-v5.3`
- `docker run --rm -e HOME=/tmp espressif/idf:release-v5.3 idf.py --version 2>&1 | tail -3`
- `   mkdir -p $REPO/firmware && docker run --rm -v "$PWD":"$PWD" -w "$PWD/firmware" -u $(id -u):$(id -g) -e HOME=/tmp espressif/idf:release-v5.3 idf.py create-project estacao 2>&1 | tail -5`
- `   make set-target 2>&1 | tail -15`
- `   make build 2>&1 | tail -20`
- `git status --short && git add CLAUDE.md PLANO.md && git commit -m "docs: adiciona CLAUDE.md e PLANO.md (guias de trabalho do projeto)" -q && git add Makefile .clangd .gitignore && git commit -m "chore: ambiente de build dockerizado (Makefile, .clangd, .gitignore)
   Toolchain via espressif/idf:release-v5.3; TTY autodetectado e --user
   com UID do host para artefatos não saírem com dono root." -q && git add firmware/ && git commit -m "feat(estacao): cria projeto ESP-IDF com hello world (contador 1 s)" -q && git add docs/00-ambiente.md && git commit -m "docs: diário de bordo da fase 0 (ambiente)" -q && git log --oneline -6 && git status --short`
- `   docker ps -a --format '{{.Names}} {{.Status}}' | head -5; ls -ld firmware/estacao/build | awk '{print $3":"$4}'`
- `   ls -l /dev/ttyACM* /dev/ttyUSB* 2>&1; echo ---; groups; echo ---; ls -d /opt/esp/idf /opt/esp/tools 2>&1; echo ---; git status --short | head -12`
- `   grep '^uucp:' /etc/group; echo ---; git log --oneline -4`
- `   git add PLANO.md && git commit -q -m "docs: fecha o checklist da fase 0" && git add firmware/estacao/main/estacao.c && git commit -q -m "style(estacao): reformata hello world (clang-format)" && git log --oneline -3`
- `   timeout 15 make monitor 2>&1 | grep -a 'estacao:' | head -8; true`
- `   timeout 15 make monitor 2>&1 | tail -20; true`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   t0 = time.time()
   while time.time() - t0 < 12:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -a estacao | head -6`
- `   git add Makefile compose.yml && git commit -q -m "fix: adiciona GID da serial como grupo suplementar no service dev
   user: do compose não carrega grupos suplementares; sem o GID do grupo
   dono do device (uucp/dialout) o /dev/ttyACM0 fica ilegível no container." && git log --oneline -1`

- `url: "https://components.espressif.com/api/components?q=bmp280", prompt: "List all components matching bmp280: their full name (namespace/name), latest version, supported ESP-IDF version constraint, and short description. Include download/star counts if present.")`
- `   url: "https://components.espressif.com/components/k0i05/esp_bmp280", prompt: "Describe this component's API: does it use the new i2c_master driver (i2c_master_bus_handle_t)? Show the initialization and read functions signatures, its dependencies, license, and a usage example if present. Also note the exact idf.py add-dependency command."`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=0 PORT=/dev/ttyACM0 docker compose run --rm -T toolchain idf.py add-dependency "k0i05/esp_bmp280^1.2.7" 2>&1 | tail -4`
- `   make build 2>&1 | grep -aE 'Solving|dependencies|esp_bmp280|error|Error' | head -10; ls firmware/estacao/managed_components/ 2>/dev/null`
- `   make flash 2>&1 | tail -2 && sleep 2 && REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   t0 = time.time()
   while time.time() - t0 < 14:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -a 'estacao:' | head -6`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)  # reset da placa
   t0 = time.time()
   while time.time() - t0 < 12:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | tail -15`
- `   make build 2>&1 | grep -aE 'error|warning.*estacao|Project build' | head -5 && make flash 2>&1 | tail -1 && sleep 1 && REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
   t0 = time.time()
   while time.time() - t0 < 10:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -a 'estacao:' | head -8`

- `   make build 2>&1 | grep -a 'Project build complete\|error' | head -3 && make flash 2>&1 | tail -1 && sleep 1 && REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
   t0 = time.time()
   while time.time() - t0 < 8:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -a 'SELFTEST\|estacao:\|sensor_bmp280:' | head -5`
- `   sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_LDR=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Project build complete\|error:' | head -3`
- `   sed -i 's/# CONFIG_ESTACAO_TESTE_TODOS is not set/CONFIG_ESTACAO_TESTE_TODOS=y/; s/CONFIG_ESTACAO_TESTE_LDR=y/# CONFIG_ESTACAO_TESTE_LDR is not set/' firmware/estacao/sdkconfig && git add firmware/estacao/main/ firmware/estacao/sdkconfig docs/02-ldr.md PLANO.md && git commit -q -m "feat(estacao): modulo sensor_ldr (ADC1 oneshot, media de 16 amostras) Software validado com jumper (3V3 -> 4095); modulo KY-018 com LDR
   soldado em furos nao conectados — pendencia de hardware em docs/02." && git log --oneline -1`
- `   sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_CHUVA=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Project build complete\|error:' | head -3`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=0 PORT=/dev/ttyACM0 docker compose run --rm -T toolchain idf.py add-dependency "esp-idf-lib/dht^1.2.0" 2>&1 | tail -2`
- `  sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_DHT11=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Processing\|esp-idf-lib\|Project build complete\|error:' | head -6`
- `   sed -i 's/# CONFIG_ESTACAO_TESTE_TODOS is not set/CONFIG_ESTACAO_TESTE_TODOS=y/; s/CONFIG_ESTACAO_TESTE_DHT11=y/# CONFIG_ESTACAO_TESTE_DHT11 is not set/' firmware/estacao/sdkconfig && git add firmware/estacao/main/ firmware/estacao/sdkconfig firmware/estacao/dependencies.lock docs/04-dht11.md && git commit -q -m "feat(estacao): modulo sensor_dht11 (esp-idf-lib/dht, GPIO 4)
   Validado na bancada: umidade/temperatura plausiveis, resposta ao bafo.
   DHT11 so reporta graus inteiros — termometro principal segue o BMP280." && git log --oneline -1`
- `   sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_VENTO=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Project build complete\|error:' | head -3`
- `   sed -i 's/# CONFIG_ESTACAO_TESTE_TODOS is not set/CONFIG_ESTACAO_TESTE_TODOS=y/; s/CONFIG_ESTACAO_TESTE_VENTO=y/# CONFIG_ESTACAO_TESTE_VENTO is not set/' firmware/estacao/sdkconfig && git add firmware/estacao/main/ firmware/estacao/sdkconfig docs/05-vento.md && git commit -q -m "feat(estacao): modulo sensor_vento (reed KY-021, ISR + debounce 10 ms)
Validado na bancada com jumper (simula reed) e ima; contador atomico
   com leitura-e-zera." && git log --oneline -1`
- `   sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_LED=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Project build complete\|error:' | head -3`
- `   sed -i 's/# CONFIG_ESTACAO_TESTE_TODOS is not set/CONFIG_ESTACAO_TESTE_TODOS=y/; s/CONFIG_ESTACAO_TESTE_LED=y/# CONFIG_ESTACAO_TESTE_LED is not set/' firmware/estacao/sdkconfig && git add firmware/estacao/main/ firmware/estacao/sdkconfig docs/06-led-rgb.md && git commit -q -m "feat(estacao): modulo led_status (LEDC 5 kHz/8 bits, R16 G17 B13)
Modulo Wcmcu: anodo comum (silkscreen '-' mentiroso -> comum no 3V3,
   output_invert no LEDC) e sem resistores (teto de duty em 25%).
   Validado: ciclo de cores e fade." && git log --oneline -1`
- `   make build 2>&1 | grep -a 'Project build complete\|error:' | head -3 && make flash 2>&1 | tail -1 && sleep 1 && make test-bancada 2>&1 | tail -12`
- `   make build 2>&1 | grep -a 'Project build complete' && make flash 2>&1 | tail -1 && sleep 1 && make test-bancada 2>&1 | tail -13`
- `   sed -i 's/CONFIG_ESTACAO_TESTE_TODOS=y/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_MODO_ESTACAO=y/' firmware/estacao/sdkconfig && make build 2>&1 | grep -a 'Project build complete\|error:' | head -3 && make flash 2>&1 | tail -1`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
   t0 = time.time()
   while time.time() - t0 < 75:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -a 'SELFTEST\|pacote seq\|estacao: 0x' | head -14`
- `   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   t0 = time.time()
   while time.time() - t0 < 35:
       line = s.readline().decode(errors='replace').rstrip()
       if line: print(line)
   " 2>&1 | grep -aA2 'pacote seq' | head -6`
- `'   REPO_ROOT=$PWD WORK_DIR=$PWD/firmware/estacao HOST_UID=$(id -u) HOST_GID=$(id -g) PORT_GID=$(stat -c %g /dev/ttyACM0) PORT=/dev/ttyACM0 docker compose run --rm -T dev python -u -c "
   import serial, time
   s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
   t0 = time.time()
   while time.time() - t0 < 40:
       line = s.readline().decode(errors='replace').rstrip()
       if line and 'estacao:' in line: print(line)
   " 2>&1 | tail -4`
- `   sed -i 's/^CONFIG_ESTACAO_TESTE_LDR=y$/# CONFIG_ESTACAO_TESTE_LDR is not set/; s/^# CONFIG_ESTACAO_TESTE_TODOS is not set$/# CONFIG_ESTACAO_TESTE_TODOS is not set\nCONFIG_ESTACAO_TESTE_BMP280=y/' firmware/estacao/sdkconfig && grep -n "ESTACAO_" firmware/estacao/sdkconfig`
- `   sed -i 's/^CONFIG_ESTACAO_TESTE_BMP280=y$/# CONFIG_ESTACAO_TESTE_BMP280 is not set/; s/^# CONFIG_ESTACAO_TESTE_LED is not set$/# CONFIG_ESTACAO_TESTE_LED is not set\nCONFIG_ESTACAO_TESTE_LORA=y/' firmware/estacao/sdkconfig && grep -n "ESTACAO_TESTE\|ESTACAO_MODO" firmware/estacao/sdkconfig`
- `   sed -i 's/^CONFIG_ESTACAO_MODO_ESTACAO=y$/# CONFIG_ESTACAO_MODO_ESTACAO is not set/' firmware/estacao/sdkconfig && sed -n '393,405p' firmware/estacao/sdkconfig && make build 2>&1 | tail -6`
- `   git ls-remote https://github.com/tapparelj/gr-lora_sdr.git HEAD`
- `   docker compose -f $REPO/infra/docker-compose.yml build sdr-receptor 2>&1 | tail -40`
- `   cd $REPO/infra && docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   import gnuradio.lora_sdr as lora_sdr
   import osmosdr
   fs = lora_sdr.frame_sync(433000000, 125000, 12, False, [0x12], 2, 8)
   fft = lora_sdr.fft_demod(True, True)
   g = lora_sdr.gray_mapping(True)
   d = lora_sdr.deinterleaver(True)
   h = lora_sdr.hamming_dec(True)
   hd = lora_sdr.header_decoder(False, 1, 255, True, 1, True)
   dw = lora_sdr.dewhitening()
   crc = lora_sdr.crc_verif(1, False)
   print('todos os blocos instanciados OK (SF12, os=2, sync 0x12, LDRO on)')
   " 2>&1 | tail -5`
- `   docker compose build sdr-receptor 2>&1 | tail -2 && docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   from gnuradio import blocks, gr
   c = blocks.copy(gr.sizeof_gr_complex)
   c.set_min_output_buffer(int(4096*2*4))
   print('copy block + buffer de', 4096*2*4, 'itens OK')
   " 2>&1 | tail -2`
- `   docker compose run --rm --no-deps --entrypoint bash sdr-receptor -c "find /usr -name 'lora_sdr' -maxdepth 6 -type d 2>/dev/null; python3 -c 'import sys; print([p for p in sys.path if \"packages\" in p])'" 2>&1 | tail -4`
- `   docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   from gnuradio import gr, blocks
   import gnuradio.lora_sdr as lora_sdr
   import time
class TB(gr.top_block):
       def __init__(self):
           super().__init__()
           src = blocks.null_source(gr.sizeof_gr_complex)
           cp = blocks.copy(gr.sizeof_gr_complex)
           cp.set_min_output_buffer(32768)
           fs = lora_sdr.frame_sync(433000000,125000,12,False,[0x12],2,8)
           snk = blocks.null_sink(gr.sizeof_gr_complex)
           self.connect(src, cp, fs, snk)
tb = TB()
   tb.start()
   time.sleep(2)
   tb.stop(); tb.wait()
   print('fim do experimento')
   " 2>&1 | grep -v "^Using\|CPU Features" | tail -6`
- ` docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   from gnuradio import gr, blocks, filter as gr_filter
   from gnuradio.filter import firdes
   import gnuradio.lora_sdr as lora_sdr
   import time, sys def roda(nome, montar):
       class TB(gr.top_block):
           def __init__(self):
               super().__init__()
               montar(self)
       tb = TB()
       tb.start(); time.sleep(2); tb.stop(); tb.wait()
       print(f'--- fim: {nome}', flush=True) # A: min_output_buffer com 2 argumentos (porta, tamanho) em todos os blocos
   def cen_a(tb):
       src = blocks.null_source(gr.sizeof_gr_complex)
       cp = blocks.copy(gr.sizeof_gr_complex)
       for b in (src, cp):
           b.set_min_output_buffer(0, 65536)
       fs = lora_sdr.frame_sync(433000000,125000,12,False,[0x12],2,8)
       tb.connect(src, cp, fs, blocks.null_sink(gr.sizeof_gr_complex)) # B: decimador FIR /2 -> frame_sync com os_factor=1 (sem buffer especial)
   def cen_b(tb):
       src = blocks.null_source(gr.sizeof_gr_complex)
       taps = firdes.low_pass(1.0, 250e3, 62.5e3, 25e3)
       dec = gr_filter.fir_filter_ccf(2, taps)
       fs = lora_sdr.frame_sync(433000000,125000,12,False,[0x12],1,8)
       tb.connect(src, dec, fs, blocks.null_sink(gr.sizeof_gr_complex))
       print(f'(taps do filtro: {len(taps)})', flush=True)
roda('A (min_output_buffer porta 0)', cen_a)
   roda('B (decimador, os=1)', cen_b)
   " 2>&1 | grep -vE "^Using|CPU Features|Creating|Created|vmcircbuf" | tail -8`
- `   docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   from gnuradio import gr, blocks
   import gnuradio.lora_sdr as lora_sdr
   import time
   def roda(nome, tam, dois_args, tambem_src):
       class TB(gr.top_block):
           def __init__(self):
               super().__init__()
               src = blocks.null_source(gr.sizeof_gr_complex)
               cp = blocks.copy(gr.sizeof_gr_complex)
               alvos = (src, cp) if tambem_src else (cp,)
               for b in alvos:
                   if dois_args: b.set_min_output_buffer(0, tam)
                   else: b.set_min_output_buffer(tam)
               fs = lora_sdr.frame_sync(433000000,125000,12,False,[0x12],2,8)
               self.connect(src, cp, fs, blocks.null_sink(gr.sizeof_gr_complex))
       tb = TB(); tb.start(); time.sleep(1.5); tb.stop(); tb.wait()
       print(f'--- fim: {nome}', flush=True)
   roda('1arg 32768 so copy (reproducao)', 32768, False, False)
   roda('1arg 65536 so copy', 65536, False, False)
   roda('2args 32768 so copy', 32768, True, False)
   " 2>&1 | grep -E "fim:|block_executor"`
- `   cd $REPO/infra && docker compose up -d mosquitto 2>&1 | tail -2 && docker compose exec mosquitto sh -c "
   mosquitto_sub -h localhost -t 'estacao/#' -C 1 -W 10 > /tmp/recebido &
   sleep 1
   mosquitto_pub -h localhost -t 'estacao/v1/teste' -m 'ola do broker da fase 5'
   wait
   echo \"assinante recebeu: \$(cat /tmp/recebido)\"
   " && docker compose logs mosquitto 2>&1 | tail -3`
- `   cd $REPO/infra && docker compose build sdr-receptor 2>&1 | tail -2 && docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   import pmt, struct # 1) formato de fio do PMT p/ simbolos (descoberta empirica) bruto = bytes(pmt.serialize_str(pmt.intern('abc')))
   print('serializacao de symbol \"abc\":', bruto.hex())
import receptor_lora as r # 2) selfcheck do CRC + extracao de payload de um simbolo ascii assert r.crc16_ccitt(b'123456789') == 0x29B1
   assert r.payload_de_pmt(pmt.intern('chirp #96')) == b'chirp #96'
   print('crc16 selfcheck + payload_de_pmt: OK') # 3) dois pacotes REAIS capturados pelo radio ontem p1 = bytes.fromhex('0300d608ef61010047d505ff0f0000000caf')
   p2 = bytes.fromhex('0400d408f261010047ba0aff0f000000f8ef')
   for p in (p1, p2):
       print('pacote real decodificado:', r.desserializar(p))
   " 2>&1 | grep -vE "^Using|CPU Features|Creating|Created" | tail -6`
- `   docker compose run --rm --entrypoint python3 sdr-receptor -c "
   import pmt, struct, time
   import receptor_lora as r # simbolo PMT binario genuino, reconstruido pelo formato de fio
   p2 = bytes.fromhex('0400d408f261010047ba0aff0f000000f8ef')
   msg = pmt.deserialize_str(bytes([0x02]) + struct.pack('>H', len(p2)) + p2)
cliente = r.criar_cliente_mqtt()
   time.sleep(1)  # conexao assincrona
   ponte = r.PonteMqtt(cliente)
   ponte.tratar(msg)                      # caminho feliz -> publica
   ponte.tratar(pmt.intern('chirp #96'))  # tamanho errado -> descarta
   time.sleep(1)
   cliente.loop_stop()
   " 2>&1 | grep -E "ponte" ; echo "--- o que ficou retido no broker:" && docker compose exec mosquitto mosquitto_sub -t 'estacao/v1/dados' -C 1 -W 5 -v`
- `   cd $REPO/infra && docker compose up -d mosquitto dashboard 2>&1 | tail -3 && echo "--- pagina servida:" && curl -s http://localhost:8080/ | head -c 120 && echo && curl -s -o /dev/null -w "mqtt.min.js: HTTP %{http_code}, %{size_download} bytes\n" http://localhost:8080/mqtt.min.js && echo "--- teste do listener websocket (assinante paho via ws):" && docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   import paho.mqtt.client as mqtt, json, time
   recebido = []
   def on_msg(c, u, m): recebido.append(m.payload.decode())
   try:
       c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, transport='websockets')
   except AttributeError:
       c = mqtt.Client(transport='websockets')
   c.on_message = on_msg
   c.connect('mosquitto', 9001, 30)
   c.subscribe('estacao/v1/dados')
   c.loop_start(); time.sleep(2); c.loop_stop()
   print('retido via websocket:', recebido[0][:80] if recebido else 'NADA')
   " 2>&1 | tail -1`
- `   docker ps --format '{{.Names}}\t{{.Status}}\t{{.Ports}}' && echo --- && docker compose logs dashboard 2>&1 | tail -3 && echo --- && docker compose exec dashboard wget -qO- http://localhost:80/ 2>&1 | head -c 150 && echo && curl -s http://127.0.0.1:8080/ 2>&1 | head -c 100; echo "curl exit: $?"`
- `   cd $REPO/infra && docker compose run --rm --no-deps --entrypoint python3 sdr-receptor -c "
   import paho.mqtt.client as mqtt, time
   recebido = []
   def on_msg(c, u, m): recebido.append(m.payload.decode())
   try:
       c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, transport='websockets')
   except AttributeError:
       c = mqtt.Client(transport='websockets')
   c.on_message = on_msg
   c.connect('mosquitto', 9001, 30)
   c.subscribe('estacao/v1/dados')
   c.loop_start(); time.sleep(2); c.loop_stop()
   print('retido via websocket:', recebido[0][:90] if recebido else 'NADA')
   " 2>&1 | tail -1`
