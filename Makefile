DEVKIT ?= $(HOME)/.devkitsms
PROG   := scorched
CC     := sdcc
CFLAGS := -mz80 -I$(DEVKIT)/include --peep-file $(DEVKIT)/include/peep-rules.txt -Isrc --max-allocs-per-node 20000
# ui.c, ai.c and the asset data live in ROM bank 2, mapped permanently at 0x8000
BANKED := --codeseg BANK2 --constseg BANK2
LDFLAGS:= -mz80 --no-std-crt0 --data-loc 0xC000 -Wl-b_BANK2=0x8000
OBJS   := main.rel terrain.rel weapons.rel ai.rel ui.rel data.rel

all: $(PROG).sms
src/data.c src/data.h: gen.py
	python3 gen.py
main.rel terrain.rel weapons.rel: %.rel: src/%.c src/data.h src/game.h
	$(CC) $(CFLAGS) -c $< -o $@
ai.rel ui.rel data.rel: %.rel: src/%.c src/data.h src/game.h
	$(CC) $(CFLAGS) $(BANKED) -c $< -o $@
.PHONY: $(PROG).ihx
$(PROG).ihx: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(DEVKIT)/lib/crt0_sms.rel $(OBJS) $(DEVKIT)/lib/SMSlib.lib $(DEVKIT)/lib/PSGlib.lib
	@python3 checkmap.py $(PROG).map
$(PROG).sms: $(PROG).ihx
	ihx2sms $< $@
playtest: $(PROG).sms
	@mkdir -p shots
	smstest $(PROG).sms tests/playtest.txt
clean:
	rm -f *.rel *.ihx *.sym *.lst *.map *.noi *.lk *.asm *.rst $(PROG).sms
