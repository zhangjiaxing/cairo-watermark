CC=gcc

all: watermark lib.so

lib.so: lib.c
	$(CC) -fPIC -shared -ldl `pkg-config --libs --cflags cairo-xlib` $^ -o $@

test:
	./watermark gtk3-widget-factory

clean:
	rm ./watermark ./lib.so

.PHONY: clean

