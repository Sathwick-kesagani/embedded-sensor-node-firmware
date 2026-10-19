# Build and test the sensor-node firmware on a PC.
#   make test   - build and run all unit tests
#   make sim    - build the host simulation binary (used by the link tests)
#   make link   - run the Python link tests against the simulation binary
#   make all    - everything

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow -O1 -g
INC     := -Ifirmware/inc -Ithird_party/unity
BUILD   := build

FW_SRC  := firmware/src/ring_buffer.c firmware/src/crc16.c firmware/src/protocol.c \
           firmware/src/sensor.c firmware/src/fault_fsm.c
UNITY   := third_party/unity/unity.c
TESTS   := ring_buffer crc16 protocol sensor fault_fsm

.PHONY: all test sim link clean sanitize
all: test sim link

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_%: tests/unit/test_%.c $(FW_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< $(FW_SRC) $(UNITY) -Wno-conversion -o $@

test: $(addprefix $(BUILD)/test_,$(TESTS))
	@set -e; for t in $^; do echo "== $$t"; ./$$t; done

sim: $(BUILD)/sensor_node_sim

$(BUILD)/sensor_node_sim: $(FW_SRC) firmware/src/app.c platform/host/hal_host.c platform/host/main_host.c | $(BUILD)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHOST_SIM $(INC) $^ -o $@

link: sim
	python3 tests/link/test_link.py

clean:
	rm -rf $(BUILD) results

# Unit tests again with AddressSanitizer + UndefinedBehaviorSanitizer
sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
