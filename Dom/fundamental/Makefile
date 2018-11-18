#ident "@(#)Makefile	1.16 23 Jun 1995 10:52:22 %n%"
 
BASEDIR = ../..
include $(BASEDIR)/domains/Makefile.master
KCPPDEP = ../kcpp/kcpp.depend
SUPDEP = ../support/support.depend

# Those in DOMAINS1 use Mapfile1h, cfstart.o and support.a
DOMAINS1 = siac fsc callsegc bscrf fcc forkfc kidc mbwait2c \
	recepc sik2sim snodec vdk2rc wait datacopy \
	snode2 pcskeepf vcsf bsloadf vdkf lclrecepf lclauthf \
	tssf mkeeperf switcherf cswitcherf devttyf devconsf
DOMAINS1_OBJ = $(DOMAINS1:%=%.o)

        
# Those in DOMAINS2 conform to ABI and use cstart.o and support.a
DOMAINS2 = pcs gnedit clockc 
DOMAINS2_OBJ= $(DOMAINS2:%=%.o)

# Those in DOMAINS3 use Mapfile1h, cfstart.o, dfalloc.o and support.a
DOMAINS3 = tdo ukeeperf pschedf
DOMAINS3_OBJ = $(DOMAINS3:%=%.o)

DOMAINS_C = $(DOMAINS1:%=%.c) $(DOMAINS2:%=%.c) $(DOMAINS3:%=%.c)

ALLDOMAINS = $(DOMAINS1) $(DOMAINS2) $(DOMAINS3)
ALLDOMAINS_OBJ = $(DOMAINS1_OBJ) $(DOMAINS2_OBJ) $(DOMAINS3_OBJ)
CFSTART = ../support/cfstart.o
CSTART = ../support/cstart.o
SUPPORT = ../support/support.a

.PRECIOUS: fundamental.depend 
.PRECIOUS: tssf.c ukeeperf.c pschedf.c devttyf.c

all: fundamental.depend

fundamental.depend: $(ALLDOMAINS_OBJ) $(ALLDOMAINS) demo2 uwrapper tarseg RootDir.tarseg Init.tarseg
	@>fundamental.depend

# We specify -IKLUDGE only as a means of making sure the linker accounts
# for the size of the ELF header and program header table when
# determining the contents of the first segment. Without it, the file
# offset of the first text symbol would not match the virtual address
# of the first text symbol. The value of the -I option is really not
# important.

$(DOMAINS1): $(SUPDEP) Mapfile1h 
	@echo $(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o $(SUPPORT)\
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 
	@$(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o $(SUPPORT) \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc 2>&1 | \
		sed "s/.*No read-only segments found.*//g"

$(DOMAINS2): $(SUPDEP)
	@echo $(LD) -dn -o $@ $(CSTART) $@.o $(SUPPORT) \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc
	$(LD) -dn -o $@ $(CSTART) $@.o $(SUPPORT)  \
  -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc -lc

$(DOMAINS3): $(SUPDEP) Mapfile1h dfalloc.o
	@echo $(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o \
		dfalloc.o $(SUPPORT) -lc
	@$(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o \
		dfalloc.o $(SUPPORT) -lc 2>&1 | \
		sed "s/.*No read-only segments found.*//g"

demo2: $(SUPDEP) demo2.o Mapfile1h newdemosn.o
	@echo $(LD) -dn -o demo2 -I KLUDGE -M Mapfile1h $(CFSTART) demo2.o \
		-L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc \
		newdemosn.o $(SUPPORT) -lc -lgcc
	@$(LD) -dn -o demo2 -I KLUDGE -M Mapfile1h $(CFSTART) demo2.o \
		-L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc\
		newdemosn.o $(SUPPORT) -lc -lgcc  2>&1 | \
		sed "s/.*No read-only segments found.*//g"

tarseg: tarseg.c
	$(CC) -o tarseg tarseg.c

uwrapper: uwrapperc.o uwrappers.o
	@echo $(LD) -dn -o uwrapper -M Mapfile2hO \
		uwrappers.o uwrapperc.o $(SUPPORT) -lc
	@$(LD) -dn -o uwrapper -M Mapfile2hO \
		uwrappers.o uwrapperc.o $(SUPPORT) -lc 2>&1 | \
		sed "s/.*No read-only segments found.*//g"

RootDir.tarseg: FORCE
	(cd ../testdomains; tar cf /tmp/RootDir.tar$$USER ./*)
	touch rootzip
	rm rootzip
	touch root.zip
	rm root.zip
	(cd ../../../lib/root; zip -0 ../../src/domains/fundamental/root `find *`)
	./tarseg /tmp/RootDir.tar$$USER RootDir.tarseg
#	rm /tmp/RootDir.tar$$USER

Init.tarseg: FORCE
#	tar cf /tmp/Init.tar$$USER $(DOMAINS1) $(DOMAINS2) $(DOMAINS3) demo2 init.cmd uwrapper -C ../../../lib root
	tar cf /tmp/Init.tar$$USER $(DOMAINS1) $(DOMAINS2) $(DOMAINS3) demo2 init.cmd uwrapper root.zip -C ../../../lib/root/usr/lib ld.so.1
	./tarseg /tmp/Init.tar$$USER Init.tarseg
#	rm /tmp/Init.tar$$USER

%.c: %.ck $(KCPPDEP)
	$(KCPP) $< $(INCS)
	mv kcpp.$< $@

%.s: %.sk $(KCPPDEP)
	$(KCPP) $< -ASM $(INCS)
	mv kcpp.$< $@

$(KCPPDEP) $(SUPDEP): FORCE
	@cd $(@D); make -e

clean:
	rm -f $(ALLDOMAINS:%=%.o) $(ALLDOMAINS:%=%.c) dfalloc.o tarseg.o demo2.o \
	newdemosn.o uwrappers.o uwrapperc.o

clobber: clean
	rm -f $(ALLDOMAINS) demo2 fundamental.depend $(MSFILES)
	rm -f tarseg RootDir.tarseg Init.tarseg uwrapper

FORCE:
