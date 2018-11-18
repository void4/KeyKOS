#ident "@(#)Makefile	1.5 22 May 1995 15:27:20 %n%"

BASEDIR = ..
include Makefile.master

DOM_DIRLIST = kcpp support fundamental primordial testdomains
#DEPLIST = $(DOM_DIRLIST:%=%/%.depend)
DEPLIST = kcpp/kcpp.depend support/support.depend \
	fundamental/fundamental.depend primordial/primordial.depend testdomains/testdomains.depend

.PRECIOUS: $(DEPLIST)

all: domains.depend

domains.depend: $(DEPLIST)
	@>domains.depend

$(DEPLIST): FORCE
	@cd $(@D); pwd; $(MAKE) -e

clobber clean:
	@for i in $(DOM_DIRLIST) ; \
	do \
		(cd $$i; $(MAKE) -e $@) ; \
	done
	@if [ "$@" = "clobber" ] ;\
	then \
		rm -f domains.depend $(MSFILES) ;\
	fi

FORCE:
