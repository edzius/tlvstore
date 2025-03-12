ifneq ($(DEBUG),)
CFLAGS += -DDEBUG -g
endif
ifneq ($(CONFIG_TLVS_FILE),)
CFLAGS += -DTLVS_DEFAULT_FILE=\"$(CONFIG_TLVS_FILE)\"
endif
ifneq ($(CONFIG_TLVS_SIZE),)
CFLAGS += -DTLVS_DEFAULT_SIZE=$(CONFIG_TLVS_SIZE)
endif

.PHONY: all
all: tlvs

.PHONY: clean
clean:
	rm *.o tlvs

.PHONY: install
install: tlvs
	install -Dm755 tlvs $(PREFIX)/usr/bin/tlvs

tlvs: main.o firmux-eeprom.o protocol.o char.o tlv.o utils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS-$<) -c -o $@ $<

main.o: main.c
firmux-eeprom.o: firmux-eeprom.c
protocol.o: protocol.c
tlv.o: tlv.c
char.o: char.c
utils.o: utils.c
