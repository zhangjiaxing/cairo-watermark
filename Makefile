all: watermark lib.so

lib.so: lib.c
	gcc -fPIC -shared -ldl `pkg-config --libs --cflags cairo-xlib` $^ -o $@

inject: inject.c
	gcc $^ -o $@

test:
	./watermark gtk3-widget-factory

clean:
	rm ./watermark ./lib.so

.PHONY: clean

