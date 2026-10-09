# MTP-F26 Team C radio firmware - host build and tests.
#   make test     regenerate nrf24_config.h from ../phy_config.py, build, run tests
CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wconversion -O1 -g
SRC = nrf24.c crc16.c
TEST_SRC = test/test_main.c test/mock_nrf24.c

.PHONY: test config clean

test: build/test_radio
	./build/test_radio

config:
	cd .. && python3 phy_config.py > /dev/null

build/test_radio: config $(SRC) $(TEST_SRC) nrf24.h crc16.h test/mock_nrf24.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(TEST_SRC)

clean:
	rm -rf build
