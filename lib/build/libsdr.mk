#
#  makefile for Pocket SDR shared library (libsdr.so)
#
#! You need to install libfftw3 as follows.
#!
#! $ pacman -S mingw-w64-x86_64-fftw (MINGW64)
#! $ sudo apt install libfftw3-dev   (Ubuntu)

CC  = gcc
SRC = ../../src
OPTIONS =
INCLUDE = -I$(SRC)
LDLIBS = -lfftw3f

ifeq ($(OS),Windows_NT)
	#! for Windows
	INSTALL = ../win32
	EXTSH = so
	OPTSH = -shared
	OPTIONS += -march=native
	#! comment out for older CPU without AVX2
	OPTIONS += -DAVX2
else
	ifeq ($(shell uname -s),Linux)
		#! for Linux
		INSTALL = ../linux
		EXTSH = so
		OPTSH = -shared
		OPTIONS += -march=native
		#! comment out for older CPU without AVX2
		OPTIONS += -DAVX2
	else ifeq ($(shell uname -s),Darwin)
		ifeq ($(shell uname -m),x86_64)
			#! for macOS Intel
			INSTALL = ../darwin_x86
		else ifeq ($(shell uname -m),arm64)
			#! for macOS Arm
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

CFLAGS = -Ofast $(INCLUDE) $(OPTIONS) -fPIC -g

OBJ = sdr_func.o sdr_cmn.o

TARGET = libsdr.$(EXTSH)

$(TARGET) : $(OBJ)
	$(CC) $(OPTSH) -o $@ $(OBJ) $(LDLIBS)

sdr_func.o : $(SRC)/sdr_func.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_func.c

sdr_cmn.o : $(SRC)/sdr_cmn.c
	$(CC) -c $(CFLAGS) $(SRC)/sdr_cmn.c

sdr_func.o : $(SRC)/pocket.h
sdr_cmn.o  : $(SRC)/pocket.h

clean:
	rm -f $(TARGET) *.o

install:
	cp $(TARGET) $(INSTALL)
