#ident "@(#)Makefile	1.16 23 Jun 1995 10:52:22 %n%"
 
BASEDIR = ../..
include $(BASEDIR)/domains/Makefile.master
KCPPDEP = ../kcpp/kcpp.depend
SUPDEP = ../support/support.depend

# Those in DOMAINS1 use Mapfile1h, cfstart.o and support.a
# Those in DOMAINS use bstart
DOMAINS  = stackt join Lorentz 

DOMAINS1 = fetest availtest switcher

DOMAINS_OBJ = $(DOMAINS:%=%.o)
DOMAINS1_OBJ = $(DOMAINS1:%=%.o)
DOMAINS_C = $(DOMAINS:%=%.c) $(DOMAINS1:%=%.c)

ALLDOMAINS = $(DOMAINS) $(DOMAINS1)
ALLDOMAINS_OBJ = $(DOMAINS_OBJ) $(DOMAINS1_OBJ)
CFSTART = ../support/cfstart.o
CSTART = ../support/cstart.o
BSTART = ../support/bstart.o
SUPPORT = ../support/support.a

.PRECIOUS: testdomains.depend
.PRECIOUS: clockc.c fetest.c availtest.c switcher.c

all: testdomains.depend

testdomains.depend: $(ALLDOMAINS_OBJ) $(ALLDOMAINS) hello sigtest lwptest threadtest clockc
	@>testdomains.depend


$(DOMAINS): $(SUPDEP) $(DOMAINS_OBJ)
	@echo $(LD) -dn -o $@ $(BSTART) $@.o $(SUPPORT)\
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 
	@$(LD) -dn -o $@ $(BSTART) $@.o $(SUPPORT) \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 2>&1 | \
		sed "s/.*No read-only segments found.*//g"

$(DOMAINS1): $(SUPDEP) $(DOMAINS1_OBJ)
	@echo $(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o $(SUPPORT)\
	  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc
	@$(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o $(SUPPORT) \
	  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 2>&1 | \
	          sed "s/.*No read-only segments found.*//g"

clockc: clockc.o $(SUPDEP)
	@echo $(LD) -dn -o $@ $(BSTART) $@.o $(SUPPORT) \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 
	@$(LD) -dn -o $@ $(BSTART) $@.o $(SUPPORT) \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 2>&1 | \
		sed "s/.*No read-only segments found.*//g"

hello: hello.c
	gcc -o hello hello.c
sigtest: sigtest.c
	gcc -o sigtest sigtest.c
lwptest: lwptest.c
	gcc -o lwptest lwptest.c
threadtest: threadtest.c
	gcc -o threadtest threadtest.c -lthread

%.c: %.ck $(KCPPDEP)
	$(KCPP) $< $(INCS)
	mv kcpp.$< $@

%.s: %.sk $(KCPPDEP)
	$(KCPP) $< -ASM $(INCS)
	mv kcpp.$< $@

$(KCPPDEP) $(SUPDEP): FORCE
	@cd $(@D); make -e

clean:
	rm -f $(ALLDOMAINS:%=%.o) $(ALLDOMAINS:%=%.c) hello.o

clobber: clean
	rm -f $(ALLDOMAINS) testdomains.depend $(MSFILES) hello

FORCE:
	rm -f foo.zip
	zip -0 foo.zip stackt*
