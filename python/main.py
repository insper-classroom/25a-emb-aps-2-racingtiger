import serial
import time
from pynput.mouse import Controller as MouseController
from pynput.keyboard import Controller as KeyboardController
from pynput.keyboard import Key

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

HEADER = 0xAA
FOOTER = 0xFF
MSG_ANALOG = 0x01

mouse = MouseController()
keyboard = KeyboardController()

acelerando = False
freiando = False
boost_ativo = False

def map_valor_para_x(val):
    screen_width = 1920
    centro = screen_width // 2
    deslocamento = int((val / 100.0) * (screen_width // 4))
    return centro + deslocamento

def read_packet(ser):
    while True:
        b = ser.read(1)
        if not b:
            return None
        if b[0] == HEADER:
            break

    msg_type = ser.read(1)[0]
    if msg_type != MSG_ANALOG:
        return None

    size = ser.read(1)[0]
    payload = ser.read(size)
    checksum = ser.read(1)[0]
    footer = ser.read(1)[0]

    if footer != FOOTER:
        return None

    chk = msg_type ^ size
    for b in payload:
        chk ^= b
    if chk != checksum:
        return None

    axis = payload[0]
    val = (payload[1] << 8) | payload[2]
    val = (val + 2**15) % 2**16 - 2**15

    return axis, val

def main():
    global acelerando, freiando, boost_ativo
    print("Controle iniciado.")
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            while True:
                pkt = read_packet(ser)
                if not pkt:
                    continue

                axis, value = pkt

                if axis == 2:
                    x = map_valor_para_x(value)
                    y = mouse.position[1]
                    mouse.position = (x, y)

                elif axis == 1:
                    # Controle com base em valor direto
                    if value == -100 and not acelerando:
                        keyboard.press('w')
                        acelerando = True
                    elif value != -100 and acelerando:
                        keyboard.release('w')
                        acelerando = False

                    if value == 100 and not freiando:
                        keyboard.press('s')
                        freiando = True
                    elif value != 100 and freiando:
                        keyboard.release('s')
                        freiando = False

                # if axis == 3:
                #     if value == 100 and not boost_ativo:
                #         keyboard.press(Key.shift)
                #         boost_ativo = True
                #     elif value != 100 and boost_ativo:
                #         keyboard.release(Key.shift)
                #         boost_ativo = False

                time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nEncerrando com segurança.")
    finally:
        if acelerando:
            keyboard.release('w')
        if freiando:
            keyboard.release('s')
        # if boost_ativo:
        #     keyboard.release(Key.shift)
        print("Programa finalizado.")

if __name__ == "__main__":
    main()
