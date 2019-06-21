scheme: scheme2.c
	${CC} -Wall scheme2.c -o scheme

.PHONY: clean

clean:
	rm -f scheme
