scheme: scheme2.c
	gcc8 -O3 -ggdb -Wall scheme2.c tokenizer.c -o scheme

.PHONY: clean

clean:
	rm -f scheme
