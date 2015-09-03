# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


.PHONY: doc
doc: info pdf ps dvi

.PHONY: info pdf ps dvi
info: bin/mds.info
pdf: bin/mds.pdf
ps: bin/mds.ps
dvi: bin/mds.dvi


obj/logo.svg: doc/logo.svg
	@printf '\e[00;01;31mCP\e[34m %s\e[00m\n' "$@"
	@mkdir -p obj
	cp $< $@
	@echo

obj/logo.pdf: doc/logo.svg
	@mkdir -p obj
	@printf '\e[00;01;31mPDF\e[34m %s\e[00m\n' "$@"
	rsvg-convert --format=pdf $< > $@
	@echo

obj/logo.eps: obj/logo.ps
	@printf '\e[00;01;31mEPS\e[34m %s\e[00m\n' "$@"
	ps2eps $<
	@echo

obj/logo.ps: doc/logo.svg
	@printf '\e[00;01;31mPS\e[34m %s\e[00m\n' "$@"
	@mkdir -p obj
	rsvg-convert --format=ps $< > $@
	@echo


bin/%.info $(foreach P,$(INFOPARTS),bin/%.info-$(P)): doc/info/%.texinfo doc/info/*.texinfo
	@printf '\e[00;01;31mTEXI\e[34m %s\e[00m\n' "$@"
	@mkdir -p bin
	$(MAKEINFO) $(TEXIFLAGS) $<
	mv $*.info $*.info-* bin
	@echo

bin/%.pdf: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.pdf
	@printf '\e[00;01;31mTEXI\e[34m %s\e[00m\n' "$@"
	@! test -d obj/pdf || rm -rf obj/pdf
	@mkdir -p obj/pdf bin
	cd obj/pdf && texi2pdf $(TEXIFLAGS) ../../$< < /dev/null
	mv obj/pdf/$*.pdf $@
	@echo

bin/%.dvi: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.eps
	@printf '\e[00;01;31mTEXI\e[34m %s\e[00m\n' "$@"
	@! test -d obj/dvi || rm -rf obj/dvi
	@mkdir -p obj/dvi bin
	cd obj/dvi && $(TEXI2DVI) $(TEXIFLAGS) ../../$< < /dev/null
	mv obj/dvi/$*.dvi $@
	@echo

bin/%.ps: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.eps
	@printf '\e[00;01;31mTEXI\e[34m %s\e[00m\n' "$@"
	@! test -d obj/ps || rm -rf obj/ps
	@mkdir -p obj/ps bin
	cd obj/ps && texi2pdf $(TEXIFLAGS) --ps ../../$< < /dev/null
	mv obj/ps/$*.ps $@
	@echo

