ifneq ($(DEBUG),)
CFLAGS += -DDEBUG -g
endif

.PHONY: all
all: tlvs

.PHONY: clean
clean:
	rm *.o tlvs

.PHONY: install
install: tlvs
	install -Dm755 tlvs $(PREFIX)/usr/bin/tlvs

tlvs: main.o firmux-eeprom.o char.o tlv.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS-$<) -c -o $@ $<

main.o: main.c
firmux-eeprom.o: firmux-eeprom.c
tlv.o: tlv.c
char.o: char.c
