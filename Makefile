CC = gcc
CFLAGS = -Wall -Wextra -g `pkg-config --cflags gtk4`
LDFLAGS = `pkg-config --libs gtk4` -pthread

all: multi_shell_app

multi_shell_app: main.o controller.o view.o model.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c controller.h
	$(CC) $(CFLAGS) -c $< -o $@

controller.o: controller.c controller.h view.h model.h
	$(CC) $(CFLAGS) -c $< -o $@

view.o: view.c view.h model.h
	$(CC) $(CFLAGS) -c $< -o $@

model.o: model.c model.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o multi_shell_app

run: multi_shell_app
	./multi_shell_app

.PHONY: all clean run
