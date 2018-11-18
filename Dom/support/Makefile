#ident "@(#)Makefile	1.15 20 Apr 1995 11:43:13 %n%"

BASEDIR = ../..
include $(BASEDIR)/domains/Makefile.master
KCPPDEP = ../kcpp/kcpp.depend

SUPOBJS = cjump.o ccvt.o llic.o sort.o bitmap.o hexcvt.o sprintf.o

.PRECIOUS: support.depend

all: support.depend

support.depend: cfstart.o cstart.o bstart.o support.a jstart.o 
	@>support.depend

support.a: support.a($(SUPOBJS))
	ar rv $@ $?
	ranlib $@

support.a(%.o): %.o
	@true

cfstart.s: cfstart.sk $(KCPPDEP)
	$(KCPP) cfstart.sk -ASM $(INCS)
	mv kcpp.cfstart.sk cfstart.s

cstart.s: cstart.sk $(KCPPDEP)
	$(KCPP) cstart.sk -ASM $(INCS)
	mv kcpp.cstart.sk cstart.s

bstart.s: bstart.sk $(KCPPDEP)
	$(KCPP) bstart.sk -ASM $(INCS)
	mv kcpp.bstart.sk bstart.s

jstarts.s: jstarts.sk $(KCPPDEP)
	$(KCPP) jstarts.sk -ASM $(INCS)
	mv kcpp.jstarts.sk jstarts.s

jstartc.c: jstartc.ck $(KCPPDEP)
	$(KCPP) jstartc.ck $(INCS)
	mv kcpp.jstartc.ck jstartc.c

jstart.o: jstarts.o jstartc.o
	$(LD) -r -o jstart.o jstartc.o jstarts.o

$(KCPPDEP): FORCE
	@cd $(@D); make -e

clean:
	rm -f cfstart.s cfstart.o cstart.s cstart.o bstart.s bstartt.o jstarts.s jstarts.o jstartc.c jstartc.o jstart.o $(SUPOBJS)

clobber: clean
	rm -f support.a support.depend $(MSFILES)

FORCE:
