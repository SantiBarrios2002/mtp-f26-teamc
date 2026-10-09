# MTP-F26 Team C - protocol code + PC simulator.
#   make test      generate sample files, build, run the pass/fail suite
#   make sim       build only;  ./build/sim sri --loss 0.1 ...  /  ./build/sim nm --nodes 12 ...
CC ?= cc
RADIO = ../radio/fw
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror
CPPFLAGS += -Isrc -Isim -I$(RADIO)
PROTO = src/link.c src/nm_gossip.c src/codec.c
RADIO_SRC = $(RADIO)/nrf24.c $(RADIO)/crc16.c
SIM = sim/sim.c sim/main.c

.PHONY: test sim sample clean

test: sim sample
	./build/sim test

sim: build/sim

build/sim: $(PROTO) $(RADIO_SRC) $(SIM) $(wildcard src/*.h sim/*.h) $(RADIO)/nrf24_config.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(PROTO) $(RADIO_SRC) $(SIM) -lz

sample: build/sample.txt build/sample_utf16.txt

build/sample.txt: tools/make_sample.py
	python3 tools/make_sample.py $@

build/sample_utf16.txt: tools/make_sample.py
	python3 tools/make_sample.py $@ --utf16

clean:
	rm -rf build
