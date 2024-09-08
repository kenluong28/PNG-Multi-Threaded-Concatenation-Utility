CC = gcc
CFLAGS = -Wall -std=c99 -g # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -std=c99 -g
LDLIBS = -lcurl -lz -pthread # "curl-config --libs" output

LIB_UTIL = zutil.o crc.o catpng.o
SRCS   = paster.c catpng.c crc.c zutil.c
OBJS   = paster.o $(LIB_UTIL)

all: paster.out

paster.out: $(OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o *.png *.out