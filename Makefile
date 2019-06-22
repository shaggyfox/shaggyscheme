scheme: scheme2.c
	gcc8 -O0 -ggdb -Wall scheme2.c -o scheme

.PHONY: clean

clean:
	rm -f scheme
