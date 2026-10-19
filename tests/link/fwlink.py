"""Host-side driver for the sensor-node UART protocol.

Frames: 0xA5 | LEN | CMD | PAYLOAD | CRC16 (CCITT-FALSE over LEN, CMD, PAYLOAD).
The same class works against the simulation binary (stdin/stdout pipes) or a
real board through a serial port object that has read()/write().
"""
import os
import select
import struct
import subprocess
import time

SOF = 0xA5
REPLY = 0x80

PING, GET_VERSION, READ_SENSORS, GET_STATUS, SET_OUTPUT, CLEAR_FAULT = 1, 2, 3, 4, 5, 6
SIM_SET_ADC, NACK = 0x70, 0x7F
STATES = {0: "INIT", 1: "NORMAL", 2: "WARNING", 3: "FAULT"}
FAULT_OVERTEMP, FAULT_UNDERVOLT, FAULT_OVERVOLT, FAULT_SENSOR = 1, 2, 4, 8
ADC_VBUS, ADC_TEMP = 0, 1


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def encode(cmd: int, payload: bytes = b"") -> bytes:
    body = bytes([len(payload), cmd]) + payload
    return bytes([SOF]) + body + struct.pack(">H", crc16(body))


def vbus_raw(mv: float) -> int:
    return round(mv * 4095 / (3300 * 11))


def temp_raw(deg_c: float) -> int:
    return round((deg_c * 10 + 500) * 4095 / 3300)


class SimLink:
    """Runs build/sensor_node_sim and talks to it over pipes."""

    def __init__(self, binary="build/sensor_node_sim"):
        self.proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL)
        self.rx = bytearray()
        time.sleep(0.05)

    def write(self, data: bytes):
        self.proc.stdin.write(data)
        self.proc.stdin.flush()

    def _fill(self, timeout):
        fd = self.proc.stdout.fileno()
        r, _, _ = select.select([fd], [], [], timeout)
        if r:
            chunk = os.read(fd, 4096)
            if chunk:
                self.rx += chunk

    def read_frame(self, timeout=0.5):
        """Return (cmd, payload) or None on timeout."""
        deadline = time.monotonic() + timeout
        while True:
            while self.rx and self.rx[0] != SOF:
                del self.rx[0]
            if len(self.rx) >= 5:
                n = self.rx[1]
                if len(self.rx) >= n + 5:
                    frame = bytes(self.rx[:n + 5])
                    del self.rx[:n + 5]
                    body, crc = frame[1:-2], struct.unpack(">H", frame[-2:])[0]
                    if crc != crc16(body):
                        raise AssertionError(f"reply with bad CRC: {frame.hex()}")
                    return body[1], body[2:]
            left = deadline - time.monotonic()
            if left <= 0:
                return None
            self._fill(left)

    def request(self, cmd, payload=b"", timeout=0.5):
        self.write(encode(cmd, payload))
        return self.read_frame(timeout)

    def close(self):
        try:
            self.proc.stdin.close()
            self.proc.wait(timeout=2)
        except Exception:
            self.proc.kill()

    # helpers ------------------------------------------------------------
    def status(self):
        cmd, p = self.request(GET_STATUS)
        assert cmd == GET_STATUS | REPLY
        state, faults, crc_err, out_on, fault_count, rx_ovf = struct.unpack(">BBHBHH", p)
        return {"state": STATES[state], "faults": faults, "crc_errors": crc_err,
                "output_on": bool(out_on), "fault_count": fault_count, "rx_overflows": rx_ovf}

    def sensors(self):
        cmd, p = self.request(READ_SENSORS)
        assert cmd == READ_SENSORS | REPLY
        mv, t, raw_v, raw_t = struct.unpack(">HhHH", p)
        return {"vbus_mv": mv, "temp_c": t / 10, "raw_vbus": raw_v, "raw_temp": raw_t}

    def set_adc(self, ch, raw):
        cmd, p = self.request(SIM_SET_ADC, bytes([ch]) + struct.pack(">H", raw))
        assert cmd == SIM_SET_ADC | REPLY and p == b"\x00"

    def wait_state(self, state, timeout=1.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            s = self.status()
            if s["state"] == state:
                return s
            time.sleep(0.02)
        raise AssertionError(f"state {state} not reached, last {s}")
