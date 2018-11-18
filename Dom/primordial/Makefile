#ident "@(#)Makefile	1.18 17 May 1995 08:57:29 %n%"

BASEDIR = ../..
include $(BASEDIR)/domains/Makefile.master

DOMAINS = wkc sbc dccc sbshivac ckptdvrc emigc eresyncc
DOMAINS_OBJ = $(DOMAINS:%=%.o)
CFSTART = ../support/cfstart.o
SUPPORT = ../support/support.a
KCPPDEP = ../kcpp/kcpp.depend
SUPDEP = ../support/support.depend

.PRECIOUS: primordial.depend

all: primordial.depend

primordial.depend: $(DOMAINS_OBJ) $(DOMAINS) lsfsim
	@>primordial.depend

# .PRECIOUS: wkc.c sbc.c dccc.c sbshivac.c dkptdvrc.c emigc.c eresyncc.c 
.PRECIOUS: primordial.depend

# We specify -IKLUDGE only as a means of making sure the linker accounts
# for the size of the ELF header and program header table when
# determining the contents of the first segment. Without it, the file
# offset of the first text symbol would not match the virtual address
# of the first text symbol. The value of the -I option is really not
# important.

$(DOMAINS): $(SUPDEP) Mapfile1h 
	@echo $(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o \
		$(SUPPORT) -lc
	@$(LD) -dn -o $@ -I KLUDGE -M Mapfile1h $(CFSTART) $@.o \
		$(SUPPORT) -lc 2>&1 |  \
		sed "s/.*No read-only segments found.*//g"

demo2: $(SUPDEP) demo2.o Mapfile1h newdemosn.o
	@echo $(LD) -dn -o demo2 -I KLUDGE -M Mapfile1h $(CFSTART) demo2.o \
                -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc \
		newdemosn.o $(SUPPORT) -lc -lgcc
	@$(LD) -dn -o demo2 -I KLUDGE -M Mapfile1h $(CFSTART) demo2.o \
               -L /netopt/gnu/lib/gcc-lib/sparc-sun-solaris2.5/2.7.2.2 -lgcc\
		newdemosn.o $(SUPPORT) -lc -lgcc  2>&1 | \
		sed "s/.*No read-only segments found.*//g"

lsfsimtest: lsfsimtest.o testcjcc.o lsfsimc.o lsfsims.o
	$(CC) -dn -o lsfsimtest lsfsimtest.o lsfsims.o lsfsimc.o testcjcc.o

testcjcc.o: testcjcc.c
	$(CC) -c testcjcc.c

lsfsim: lsfsims.o lsfsimc.o 
	@echo $(LD)  -dn -o lsfsim -I KLUDGE -M Mapfile1h lsfsims.o \
		lsfsimc.o $(SUPPORT) -lc
	@$(LD)  -dn -o lsfsim -I KLUDGE -M Mapfile1h lsfsims.o \
		lsfsimc.o $(SUPPORT) -lc 2>&1 |  \
		sed "s/.*No read-only segments found.*//g"

dccc.o wkc.o ckptdvrc.o:= CPPFLAGS += -DWOMB

%.c: %.ck $(KCPPDEP)
	$(KCPP) $< $(INCS)
	mv kcpp.$< $@

%.s: %.sk $(KCPPDEP)
	$(KCPP) $< -ASM $(INCS)
	mv kcpp.$< $@

$(KCPPDEP) $(SUPDEP): FORCE
	@cd $(@D); make -e

clean:
	rm -f $(DOMAINS:%=%.o) $(DOMAINS:%=%.c) lsfsimc.o lsfsimc.c \
	newdemosn.o lsfsims.o lsfsims.s 

clobber: clean
	rm -f $(DOMAINS) lsfsim  primordial.depend $(MSFILES)

FORCE:
