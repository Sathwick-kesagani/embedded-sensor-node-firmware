# Embedded Sensor-Node Firmware (C) with Unit and Link Tests

[![Firmware tests](https://github.com/Sathwick-kesagani/embedded-sensor-node-firmware/actions/workflows/ci.yml/badge.svg)](https://github.com/Sathwick-kesagani/embedded-sensor-node-firmware/actions/workflows/ci.yml)
![C11](https://img.shields.io/badge/C-C11-blue)
![Tests](https://img.shields.io/badge/tests-33%20unit%20%2B%2016%20link-brightgreen)
![License](https://img.shields.io/badge/License-MIT-yellow)

Bare-metal style firmware for a protected sensor node, written in portable C11 behind a
hardware abstraction layer (HAL). It reads a bus voltage and a temperature sensor through the
ADC, filters the readings, runs a protection state machine that switches off an output on
over-temperature, under/over-voltage or a broken sensor, and answers commands over a framed
UART protocol with CRC-16.

The firmware is tested two ways, the same way a firmware test team would:

1. **Unit tests** (33, Unity framework) for each module, also run under AddressSanitizer and UBSan.
2. **Link tests** (16, Python) that talk to the running firmware over its UART protocol, inject
   faults, and check the replies, like a production test station or HIL rig would.

> **Scope:** the firmware currently runs as a PC process through `platform/host`, a HAL that maps
> the UART to stdin/stdout and simulates the ADC. No microcontroller is involved yet; porting the
> HAL to an STM32 board is the next step (see roadmap).

## Architecture

```text
   firmware/ (portable C, no OS, no malloc, integer math only)
 +--------------------------------------------------------------+
 |  app.c        command dispatch + 10 ms sampling loop         |
 |    |-- protocol.c   frame parser (state machine) + encoder   |
 |    |-- crc16.c      CRC-16/CCITT-FALSE                        |
 |    |-- ring_buffer.c  UART RX FIFO (ISR -> main loop)         |
 |    |-- sensor.c     moving average, ADC -> mV / 0.1 degC      |
 |    '-- fault_fsm.c  INIT / NORMAL / WARNING / FAULT (latched) |
 +------------------------------|-------------------------------+
                                | hal.h
              +-----------------+------------------+
              |                                    |
     platform/host (done)                 platform/stm32 (roadmap)
     UART = stdin/stdout                  USART + DMA/IRQ
     ADC  = simulated values              ADC1 channels
              |
     tests/link/test_link.py  <-- drives the firmware over the protocol
```

## UART protocol

`0xA5 | LEN | CMD | PAYLOAD[LEN] | CRC_HI | CRC_LO` (CRC over LEN, CMD, PAYLOAD). Replies use `CMD | 0x80`.

| Cmd | Name | Request | Reply |
|---|---|---|---|
| 0x01 | PING | - | `"PONG"` |
| 0x02 | GET_VERSION | - | major, minor, patch |
| 0x03 | READ_SENSORS | - | vbus mV, temp 0.1 degC, raw ADC x2 |
| 0x04 | GET_STATUS | - | state, fault flags, CRC errors, output, fault count, RX overflows |
| 0x05 | SET_OUTPUT | 0/1 | 0 = OK, 1 = refused (in FAULT) |
| 0x06 | CLEAR_FAULT | - | 0 = cleared, 1 = refused (condition still present) |
| 0x70 | SIM_SET_ADC | ch, raw (u16) | host simulation build only, used for fault injection |
| 0x7F | NACK | | original cmd, reason (unknown / bad length / bad argument) |

## Protection limits

| Condition | Warning | Fault (latched, output off) |
|---|---|---|
| Temperature | >= 70.0 degC (exits below 65.0) | >= 85.0 degC |
| Bus voltage | < 11.0 V or > 13.0 V | < 10.0 V or > 14.0 V |
| Temperature sensor | | ADC at a rail (open or short) |

A fault must persist for 3 consecutive samples (30 ms) so a single glitch does not trip it.

## What the tests cover

**Unit tests** (`tests/unit/`): ring buffer order, full/overflow and 16-bit index wrap; CRC check
value and every single-bit error; protocol round trip, bad CRC, oversized length, re-sync after
noise, back-to-back frames, stalled-frame timeout, max payload; ADC scaling and filter; FSM
debounce, hysteresis, latching, clear refused/accepted, sensor fault, boundaries.

**Link tests** (`tests/link/`): ping, version, NACKs, bad CRC counted with no reply, re-sync after
line noise, a 200-command burst, readings tracking injected values, output control, warning band,
over-temperature trip and latch and clear, under- and over-voltage, open sensor, and a short
glitch that must not trip. Results go to `results/link_results.csv` and `results/link_report.md`.

### A bug the link tests found

The first version of `test_07_burst_of_200_pings` got only 51 of 200 replies. The main loop copied
every waiting UART byte into the 256-byte RX ring buffer before parsing any of them, so a
1,000-byte burst overflowed it and frames were lost. The fix only takes bytes while the buffer has
room, and `GET_STATUS` now reports an RX overflow counter so a test can catch this in future.

## Run it

Needs `gcc`, `make` and Python 3 (standard library only).

```bash
make test       # 33 unit tests
make sanitize   # same tests with AddressSanitizer + UBSan
make link       # build the host firmware and run the 16 link tests
```

GitHub Actions runs all three on every push and attaches the link test report.

## Roadmap

- [ ] `platform/stm32`: port the HAL to an STM32 Nucleo board (USART2 with RX interrupt, ADC1, GPIO)
- [ ] Run the same `tests/link` suite against the board through a USB serial port
- [ ] Add FreeRTOS tasks for sampling and communication
- [ ] Add a hardware watchdog and a test that proves the board resets when the main loop hangs

## Repository layout

```text
firmware/inc, firmware/src   portable firmware modules
platform/host                HAL for running on a PC + main loop
tests/unit                   Unity unit tests
tests/link                   Python protocol driver (fwlink.py) and link tests
third_party/unity            Unity test framework (MIT)
```
