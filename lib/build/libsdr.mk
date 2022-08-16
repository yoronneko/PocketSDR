#
#  makefile for Pocket SDR GNSS-SDR library (libsdr.so, libsdr.a)
#
#! You need to install libfftw3 as follows.
#!
#! $ pacman -S mingw-w64-x86_64-fftw (MINGW64)
#! $ sudo apt install libfftw3-dev   (Ubuntu)

CC  = gcc
SRC = ../../src
<<<<<<< HEAD
OPTIONS =
INCLUDE = -I$(SRC)
LDLIBS = -lfftw3f

ifeq ($(OS),Windows_NT)
	INSTALL = ../win32
	EXTSH = so
	OPTSH = -shared
	OPTIONS += -march=native
	#! comment out for older CPU without AVX2
	OPTIONS += -DAVX2
else
	ifeq ($(shell uname -s),Linux)
		INSTALL = ../linux
		EXTSH = so
		OPTSH = -shared
		OPTIONS += -march=native
		#! comment out for older CPU without AVX2
		OPTIONS += -DAVX2
	else ifeq ($(shell uname -s),Darwin)
		ifeq ($(shell uname -m),x86_64)
			INSTALL = ../darwin_x86
		else ifeq ($(shell uname -m),arm64)
			INSTALL = ../darwin_arm
			INCLUDE += -I/opt/homebrew/Cellar/fftw/3.3.10/include
			INCLUDE += -I/opt/homebrew/Cellar/libusb/1.0.25/include
			LDLIBS  += -L/opt/homebrew/Cellar/fftw/3.3.10/lib
			LDLIBS  += -L/opt/homebrew/Cellar/libusb/1.0.25/lib
		endif
		EXTSH = dylib
		OPTSH = -dynamiclib
	endif
endif

INCLUDE = -I$(SRC) -I../RTKLIB/src
#CFLAGS = -Ofast -march=native $(INCLUDE) $(OPTIONS) -Wall -fPIC -g
CFLAGS = -Ofast -mavx2 -mfma $(INCLUDE) $(OPTIONS) -Wall -fPIC -g

OBJ = sdr_cmn.o sdr_func.o sdr_code.o sdr_code_gal.o

TARGET = libsdr.$(EXTSH)

$(TARGET) : $(OBJ)
	$(CC) $(OPTSH) -o $@ $(OBJ) $(LDLIBS)

all : $(TARGET)

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
