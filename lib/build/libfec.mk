#
#  makefile of LIBFEC shared library (libfec.so)
#
#! You need to install LIBFEC source tree as follows.
#!
#! $ git clone https://github.com/quiet/libfec libfec

#! specify directory of LIBFEC source tree
SRC = ../libfec

ifeq ($(OS),Windows_NT)
    INSTALL = ../win32
else
    INSTALL = ../linux
endif

TARGET = libfec.so libfec.a

ifeq ($(shell uname -s),Darwin)
	ifeq ($(shell uname -m),x86_64)
		INSTALL = ../darwin_x86
		CONFOPT = --target=x86-apple-darwin --build=x86-apple-darwin
	else ifeq ($(shell uname -m),arm64)
		INSTALL = ../darwin_arm
	endif
	TARGET = libfec.dylib
else
	CONFOPT =
endif

all :
	DIR=`pwd`; \
	cd $(SRC); \
	./configure $(CONFOPT); \
	sed 's/-lc//' < makefile > makefile.p; \
	mv makefile.p makefile; \
	make; \
	cd $$DIR; \
	cp $(SRC)/libfec.so $(SRC)/libfec.a .

clean:
	DIR=`pwd`; \
	cd $(SRC); \
	make clean; \
	cd $$DIR; \
	rm -f $(TARGET)

install:
	cp $(TARGET) $(INSTALL)
