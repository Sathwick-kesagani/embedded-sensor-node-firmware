"""Link tests: drive the firmware over its UART protocol like a test station would.

Runs against build/sensor_node_sim (the firmware compiled for the PC).
Writes results/link_results.csv and results/link_report.md.
Usage: make link
"""
import csv
import os
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(__file__))
from fwlink import (ADC_TEMP, ADC_VBUS, CLEAR_FAULT, FAULT_OVERTEMP, FAULT_OVERVOLT,  # noqa: E402
                    FAULT_SENSOR, FAULT_UNDERVOLT, GET_VERSION, NACK, PING, REPLY,
                    SET_OUTPUT, SimLink, encode, temp_raw, vbus_raw)

SETTLE = 0.25  # moving average (8 x 10 ms) + debounce (3 x 10 ms) with margin


class LinkTests(unittest.TestCase):
    def setUp(self):
        self.fw = SimLink()
        self.fw.wait_state("NORMAL")

    def tearDown(self):
        self.fw.close()

    # communication -----------------------------------------------------
    def test_01_ping(self):
        self.assertEqual(self.fw.request(PING), (PING | REPLY, b"PONG"))

    def test_02_version(self):
        cmd, p = self.fw.request(GET_VERSION)
        self.assertEqual(cmd, GET_VERSION | REPLY)
        self.assertEqual(tuple(p), (1, 0, 0))

    def test_03_unknown_command_is_nacked(self):
        self.assertEqual(self.fw.request(0x42), (NACK, bytes([0x42, 0x01])))

    def test_04_wrong_length_is_nacked(self):
        self.assertEqual(self.fw.request(PING, b"\x00"), (NACK, bytes([PING, 0x02])))

    def test_05_bad_crc_gets_no_reply_and_is_counted(self):
        bad = bytearray(encode(PING))
        bad[-1] ^= 0xFF
        self.fw.write(bytes(bad))
        self.assertIsNone(self.fw.read_frame(timeout=0.2))
        self.assertEqual(self.fw.status()["crc_errors"], 1)

    def test_06_resync_after_line_noise(self):
        self.fw.write(bytes([0x00, 0xFF, 0x13, 0x37]) + encode(PING))
        self.assertEqual(self.fw.read_frame(), (PING | REPLY, b"PONG"))

    def test_07_burst_of_200_pings(self):
        self.fw.write(b"".join(encode(PING) for _ in range(200)))
        got = 0
        while self.fw.read_frame(timeout=0.5) == (PING | REPLY, b"PONG"):
            got += 1
            if got == 200:
                break
        self.assertEqual(got, 200)
        self.assertEqual(self.fw.status()["rx_overflows"], 0)

    # measurements -------------------------------------------------------
    def test_08_nominal_readings(self):
        s = self.fw.sensors()
        self.assertAlmostEqual(s["vbus_mv"], 12000, delta=30)
        self.assertAlmostEqual(s["temp_c"], 25.0, delta=0.3)

    def test_09_readings_track_input(self):
        self.fw.set_adc(ADC_VBUS, vbus_raw(12500))
        self.fw.set_adc(ADC_TEMP, temp_raw(40.0))
        time.sleep(SETTLE)
        s = self.fw.sensors()
        self.assertAlmostEqual(s["vbus_mv"], 12500, delta=30)
        self.assertAlmostEqual(s["temp_c"], 40.0, delta=0.3)

    # output control and protection -----------------------------------------
    def test_10_output_on_off(self):
        self.assertEqual(self.fw.request(SET_OUTPUT, b"\x01"), (SET_OUTPUT | REPLY, b"\x00"))
        self.assertTrue(self.fw.status()["output_on"])
        self.assertEqual(self.fw.request(SET_OUTPUT, b"\x00"), (SET_OUTPUT | REPLY, b"\x00"))
        self.assertFalse(self.fw.status()["output_on"])

    def test_11_warning_keeps_output_on(self):
        self.fw.request(SET_OUTPUT, b"\x01")
        self.fw.set_adc(ADC_TEMP, temp_raw(75.0))
        s = self.fw.wait_state("WARNING")
        self.assertTrue(s["output_on"])

    def test_12_overtemp_trips_and_latches(self):
        self.fw.request(SET_OUTPUT, b"\x01")
        self.fw.set_adc(ADC_TEMP, temp_raw(95.0))
        s = self.fw.wait_state("FAULT")
        self.assertEqual(s["faults"], FAULT_OVERTEMP)
        self.assertFalse(s["output_on"])
        self.assertEqual(self.fw.request(SET_OUTPUT, b"\x01"), (SET_OUTPUT | REPLY, b"\x01"))
        self.assertEqual(self.fw.request(CLEAR_FAULT), (CLEAR_FAULT | REPLY, b"\x01"))
        self.fw.set_adc(ADC_TEMP, temp_raw(25.0))
        time.sleep(SETTLE)
        self.assertEqual(self.fw.status()["state"], "FAULT")  # still latched
        self.assertEqual(self.fw.request(CLEAR_FAULT), (CLEAR_FAULT | REPLY, b"\x00"))
        s = self.fw.status()
        self.assertEqual(s["state"], "NORMAL")
        self.assertEqual(s["fault_count"], 1)

    def test_13_undervoltage_trips(self):
        self.fw.set_adc(ADC_VBUS, vbus_raw(9000))
        self.assertEqual(self.fw.wait_state("FAULT")["faults"], FAULT_UNDERVOLT)

    def test_14_overvoltage_trips(self):
        self.fw.set_adc(ADC_VBUS, vbus_raw(15000))
        self.assertEqual(self.fw.wait_state("FAULT")["faults"], FAULT_OVERVOLT)

    def test_15_open_temperature_sensor_trips(self):
        self.fw.set_adc(ADC_TEMP, 0)
        self.assertEqual(self.fw.wait_state("FAULT")["faults"], FAULT_SENSOR)

    def test_16_short_glitch_does_not_trip(self):
        self.fw.set_adc(ADC_VBUS, vbus_raw(9000))
        time.sleep(0.012)  # roughly one sample period
        self.fw.set_adc(ADC_VBUS, vbus_raw(12000))
        time.sleep(SETTLE)
        s = self.fw.status()
        self.assertNotEqual(s["state"], "FAULT")
        self.assertEqual(s["fault_count"], 0)


class Recorder(unittest.TextTestResult):
    rows = []

    def _rec(self, test, result, msg=""):
        Recorder.rows.append({"test": test.id().split(".")[-1], "result": result,
                              "seconds": round(time.monotonic() - self._t0, 3), "detail": msg})

    def startTest(self, test):
        self._t0 = time.monotonic()
        super().startTest(test)

    def addSuccess(self, test):
        super().addSuccess(test)
        self._rec(test, "PASS")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self._rec(test, "FAIL", str(err[1]).splitlines()[0])

    def addError(self, test, err):
        super().addError(test, err)
        self._rec(test, "ERROR", str(err[1]).splitlines()[0])


def write_reports(rows):
    os.makedirs("results", exist_ok=True)
    with open("results/link_results.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["test", "result", "seconds", "detail"])
        w.writeheader()
        w.writerows(rows)
    passed = sum(r["result"] == "PASS" for r in rows)
    lines = ["# Firmware link test report", "",
             f"- Target: host simulation build (`build/sensor_node_sim`)",
             f"- Run: {time.strftime('%Y-%m-%d %H:%M:%S')}",
             f"- Result: **{passed}/{len(rows)} passed**", "",
             "| Test | Result | Time (s) | Detail |", "|---|---|---|---|"]
    lines += [f"| {r['test']} | {r['result']} | {r['seconds']} | {r['detail']} |" for r in rows]
    with open("results/link_report.md", "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(LinkTests)
    result = unittest.TextTestRunner(verbosity=2, resultclass=Recorder).run(suite)
    write_reports(Recorder.rows)
    sys.exit(0 if result.wasSuccessful() else 1)
