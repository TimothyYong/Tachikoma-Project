CC = g++
CFLAGS = -pedantic -Wall -fPIC --std=c++11 -O3 -g `pkg-config --cflags opencv`
LIBS = -larmadillo `pkg-config --libs opencv` -pthread
SERIAL = $(shell pwd)/../serial
BASEROBOT = $(shell pwd)/..
INCLUDE = -I$(SERIAL) -I$(BASEROBOT)
SUBDIRS = $(SERIAL)
SUBOBJS = $(SERIAL)/serial.o
OBJECTS = armv1.o

all: subdirs $(OBJECTS)

subdirs:
	make -C $(BASEROBOT) baserobot.o
	$(foreach directory, $(SUBDIRS), make -C $(directory);)

armv1.o: armv1.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

test.o: test.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

test: $(BASEROBOT)/baserobot.o $(OBJECTS) $(SERIAL)/serial.o test.o
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJECTS) $(SUBOBJS) test.o test
	$(foreach directory, $(SUBDIRS), make clean -C $(directory);)
