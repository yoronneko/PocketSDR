# makefile for str2str

BINDIR = /usr/local/bin
SRC    = ../../../../src

# for beagleboard
#CTARGET= -mfpu=neon -mfloat-abi=softfp -ffast-math
CTARGET=

OPTION = -DENAGLO -DENAGAL -DENAQZS -DENACMP -DENAIRN -DTRACE -DNFREQ=3 -DNEXOBS=3
#OPTION = -DENAGLO -DENAGAL -DENAQZS -DENACMP -DENAIRN -DTRACE -DNFREQ=5 -DNEXOBS=3 -DSVR_REUSEADDR
CFLAGS = -Wall -O3 -ansi -pedantic -fno-common -I$(SRC) $(OPTION) $(CTARGET) -g
LDLIBS  = -lm -lpthread #-lrt

all: librtk.dylib
librtk.dylib: stream.o rtkcmn.o solution.o sbas.o geoid.o
librtk.dylib: rcvraw.o novatel.o ublox.o ss2.o crescent.o skytraq.o javad.o
librtk.dylib: nvs.o binex.o rt17.o rtcm.o rtcm2.o rtcm3.o rtcm3e.o preceph.o
librtk.dylib: streamsvr.o septentrio.o
librtk.dylib  :
	$(CC) -dynamiclib -o librtk.dylib $(CFLAGS) \
		stream.o rtkcmn.o solution.o sbas.o geoid.o \
		rcvraw.o novatel.o ublox.o ss2.o crescent.o skytraq.o javad.o \
		nvs.o binex.o rt17.o rtcm.o rtcm2.o rtcm3.o rtcm3e.o preceph.o \
		streamsvr.o septentrio.o

stream.o   : $(SRC)/stream.c
	$(CC) -c $(CFLAGS) $(SRC)/stream.c
streamsvr.o: $(SRC)/streamsvr.c
	$(CC) -c $(CFLAGS) $(SRC)/streamsvr.c
rtkcmn.o   : $(SRC)/rtkcmn.c
	$(CC) -c $(CFLAGS) $(SRC)/rtkcmn.c
solution.o : $(SRC)/solution.c
	$(CC) -c $(CFLAGS) $(SRC)/solution.c
sbas.o     : $(SRC)/sbas.c
	$(CC) -c $(CFLAGS) $(SRC)/sbas.c
geoid.o    : $(SRC)/geoid.c
	$(CC) -c $(CFLAGS) $(SRC)/geoid.c
rcvraw.o   : $(SRC)/rcvraw.c
	$(CC) -c $(CFLAGS) $(SRC)/rcvraw.c
novatel.o  : $(SRC)/rcv/novatel.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/novatel.c
ublox.o    : $(SRC)/rcv/ublox.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/ublox.c
ss2.o      : $(SRC)/rcv/ss2.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/ss2.c
crescent.o : $(SRC)/rcv/crescent.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/crescent.c
skytraq.o  : $(SRC)/rcv/skytraq.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/skytraq.c
javad.o    : $(SRC)/rcv/javad.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/javad.c
nvs.o      : $(SRC)/rcv/nvs.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/nvs.c
binex.o    : $(SRC)/rcv/binex.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/binex.c
rt17.o     : $(SRC)/rcv/rt17.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/rt17.c
rtcm.o     : $(SRC)/rtcm.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm.c
rtcm2.o    : $(SRC)/rtcm2.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm2.c
rtcm3.o    : $(SRC)/rtcm3.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3.c
rtcm3e.o   : $(SRC)/rtcm3e.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3e.c
preceph.o  : $(SRC)/preceph.c
	$(CC) -c $(CFLAGS) $(SRC)/preceph.c
septentrio.o: $(SRC)/rcv/septentrio.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/septentrio.c

str2str.o  : $(SRC)/rtklib.h
stream.o   : $(SRC)/rtklib.h
streamsvr.o: $(SRC)/rtklib.h
rtkcmn.o   : $(SRC)/rtklib.h
solution.o : $(SRC)/rtklib.h
sbas.o     : $(SRC)/rtklib.h
geoid.o    : $(SRC)/rtklib.h
rcvraw.o   : $(SRC)/rtklib.h
novatel.o  : $(SRC)/rtklib.h
ublox.o    : $(SRC)/rtklib.h
ss2.o      : $(SRC)/rtklib.h
crescent.o : $(SRC)/rtklib.h
skytraq.o  : $(SRC)/rtklib.h
javad.o    : $(SRC)/rtklib.h
nvs.o      : $(SRC)/rtklib.h
binex.o    : $(SRC)/rtklib.h
rt17.o     : $(SRC)/rtklib.h
rtcm.o     : $(SRC)/rtklib.h
rtcm2.o    : $(SRC)/rtklib.h
rtcm3.o    : $(SRC)/rtklib.h
rtcm3e.o   : $(SRC)/rtklib.h
preceph.o  : $(SRC)/rtklib.h
septentrio.o: $(SRC)/rtklib.h

clean:
	rm -f librtk.dylib *.o *.out *.trace

