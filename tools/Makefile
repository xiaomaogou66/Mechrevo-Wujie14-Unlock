CC = gcc
CFLAGS = -O2 -Wall
LIBS = -lusb-1.0

all: rtk-clear rtk-test rtk-test2 rtk-test3

rtk-clear: rtk-clear.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

rtk-test: rtk-test.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

rtk-test2: rtk-test2.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

rtk-test3: rtk-test3.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f rtk-clear rtk-test rtk-test2 rtk-test3
