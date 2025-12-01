build: main.c
	gcc -o wllock `pkg-config --cflags --libs gtk4 gtk4-layer-shell-0` -lpam main.c
