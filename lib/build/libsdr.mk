#
#  makefile for Pocket SDR GNSS-SDR library (libsdr.so, libsdr.a)
#
#! You need to install libfftw3 as follows.
#!
#! $ pacman -S mingw-w64-x86_64-fftw (MINGW64)
#! $ sudo apt install libfftw3-dev   (Ubuntu)

CC  = gcc
SRC = ../../src

ifeq ($(OS),Windows_NT)
    INSTALL = ../win32
    OPTIONS = -DWIN32 -DAVX2
    LDLIBS = ./librtk.so -lfftw3f -lwinmm
else
    INSTALL = ../linux
    OPTIONS = -DAVX2
    #OPTIONS = -DAVX512
    LDLIBS = ./librtk.a -lfftw3f
endif

INCLUDE = -I$(SRC) -I../RTKLIB/src
#CFLAGS = -Ofast -march=native $(INCLUDE) $(OPTIONS) -Wall -fPIC -g
CFLAGS = -Ofast -mavx2 -mfma $(INCLUDE) $(OPTIONS) -Wall -fPIC -g
#CFLAGS = -Ofast -mavx512f $(INCLUDE) $(OPTIONS) -Wall -fPIC -g

OBJ = sdr_cmn.o sdr_func.o sdr_code.o sdr_code_gal.o

TARGET = libsdr.so

ifeq ($(shell uname -s),Darwin)
	LDLIBS = ./librtk.dylib
	OPTSH = -dynamiclib
	TARGET = libsdr.dylib
	LDLIBS += ./librtk.dylib -lfftw3f
	ifeq ($(shell uname -m),x86_64)
		INSTALL = ../darwin_x86
		INCLUDE += -I/usr/local/Cellar/fftw/3.3.10_1/include
		INCLUDE += -I/usr/local/Cellar/libusb/1.0.26/include
		LDLIBS  += -L/usr/local/Cellar/fftw/3.3.10_1/lib
		LDLIBS  += -L/usr/local/Cellar/libusb/1.0.26/lib
	else ifeq ($(shell uname -m),arm64)
		INSTALL = ../darwin_arm
		INCLUDE += -I/opt/homebrew/include
		INCLUDE += -I/opt/homebrew/include/libusb-1.0
		LDLIBS  += -L/opt/homebrew/lib
		LDLIBS  += -L/opt/homebrew/lib/libusb-1.0
		OPTIONS =
		# clear AVX2 definition
		CFLAGS = -Ofast $(INCLUDE) $(OPTIONS) -Wall -fPIC -g
	endif
endif

$(TARGET) : $(OBJ)
	$(CC) $(OPTSH) -o $@ $(OBJ) $(LDLIBS)

all : $(TARGET)

libsdr.so: $(OBJ)
	$(CC) -shared -o $@ $(OBJ) $(LDLIBS)

sdr_cmn.o : $(SRC)/sdr_cmn.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_cmn.c

sdr_func.o : $(SRC)/sdr_func.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_func.c

sdr_code.o : $(SRC)/sdr_code.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_code.c

sdr_code_gal.o : $(SRC)/sdr_code_gal.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_code_gal.c

sdr_cmn.o  : $(SRC)/pocket_sdr.h
sdr_func.o : $(SRC)/pocket_sdr.h
sdr_code.o : $(SRC)/pocket_sdr.h

clean:
	rm -f $(TARGET) *.o

install:
	cp $(TARGET) $(INSTALL)
