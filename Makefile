CFLAGS=-g -Wall
LDFLAGS=-pthread -lpng

.PHONY : all clean

all : ipv4_anim

ipv4_anim : ipv4_anim.o hilbert.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean :
	-rm -f ipv4_anim *.o

