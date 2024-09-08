CC=gcc
CFLAGS=-Wall -g -std=c99
LD=gcc
LDFLAGS=-g
LDLIBS = -lz   # link with libz

LIB_UTIL= crc.o zutil.o

OBJS=pnginfo.o $(LIB_UTIL)
OBJS_findpng=findpng.o $(LIB_UTIL)
OBJS_catpng=catpng.o $(LIB_UTIL)


all: pnginfo.out findpng.out catpng.out

pnginfo.out: $(OBJS)
	$(LD) -o $@ $^ $(CFLAGS) $(LDLIBS) $(LDFLAGS)

findpng.out: $(OBJS_findpng)
	$(LD) -o $@ $^ $(CFLAGS) $(LDLIBS) $(LDFLAGS)

catpng.out: $(OBJS_catpng)
	$(LD) -o $@ $^ $(CFLAGS) $(LDLIBS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o *.out
