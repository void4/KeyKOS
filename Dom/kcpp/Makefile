#ident "@(#)Makefile	1.8 03 May 1995 10:13:08 %n%"

COBJECTS = kcpp.o eval.o codegen.o
CC = gcc

.PRECIOUS: kcpp.depend

all: kcpp.depend

kcpp.depend: kcpp
	@>kcpp.depend

kcpp: $(COBJECTS)
	$(CC) -o kcpp $(COBJECTS)

clean:
	rm -f $(COBJECTS)

clobber: clean
	rm -f kcpp kcpp.depend .make.*

FORCE:
