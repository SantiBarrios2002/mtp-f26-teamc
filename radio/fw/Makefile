# MTP-F26 Team C radio firmware - host build and tests.
#   make test     regenerate rfm69_config.h from ../phy_config.py, build, run tests
CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wconversion -O1 -g
SRC = rfm69.c dc_guard.c crc16.c
TEST_SRC = test/test_main.c test/mock_sx1231.c

.PHONY: test config clean

test: build/test_radio
	./build/test_radio

config:
	cd .. && python3 phy_config.py > /dev/null

build/test_radio: config $(SRC) $(TEST_SRC) rfm69.h dc_guard.h crc16.h test/mock_sx1231.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(TEST_SRC)

clean:
	rm -rf build
