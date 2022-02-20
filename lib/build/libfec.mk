#
#  makefile of LIBFEC shared library (libfec.so)
#
#! You need to install LIBFEC source tree as follows.
#!
#! $ git clone https://github.com/quiet/libfec libfec

#! specify directory of LIBFEC source tree
SRC = ../libfec

CONFOPT =
ifeq ($(OS),Windows_NT)
	#! for Windows
	INSTALL = ../win32
	EXTSH = so
else
	ifeq ($(shell uname -s),Linux)
		#! for Linux
		INSTALL = ../linux
		EXTSH = so
	else ifeq ($(shell uname -s),Darwin)
		ifeq ($(shell uname -m),x86_64)
			#! for macOS Intel
			INSTALL = ../darwin_x86
			CONFOPT = --target=x86-apple-darwin --build=x86-apple-darwin
		else ifeq ($(shell uname -m),arm64)
			#! for macOS Arm
			INSTALL = ../darwin_arm
		endif
		EXTSH = dylib
	endif
endif

TARGET = libfec.$(EXTSH)

$(TARGET) :
	DIR=`pwd`; \
	cd $(SRC); \
	./configure $(CONFOPT); \
	sed 's/-lc//' < makefile > makefile.p; \
	mv makefile.p makefile; \
	make; \
	cd $$DIR; \
	cp $(SRC)/$@ .

clean:
	DIR=`pwd`; \
	cd $(SRC); \
	make clean; \
	cd $$DIR; \
	rm -f $(TARGET)

install:
	cp $(TARGET) $(INSTALL)
