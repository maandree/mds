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
	@mkdir -p obj
	cp $< $@

obj/logo.pdf: doc/logo.svg
	@mkdir -p obj
	rsvg-convert --format=pdf $< > $@

obj/logo.eps: obj/logo.ps
	ps2eps $<

obj/logo.ps: doc/logo.svg
	@mkdir -p obj
	rsvg-convert --format=ps $< > $@


bin/%.info bin/%.info-1 bin/%.info-2: doc/info/%.texinfo doc/info/*.texinfo
	@mkdir -p bin
	$(MAKEINFO) $(TEXIFLAGS) $<
	mv $*.info $*.info-* bin

bin/%.pdf: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.pdf
	@! test -d obj/pdf || rm -rf obj/pdf
	@mkdir -p obj/pdf bin
	cd obj/pdf && yes X | texi2pdf $(TEXIFLAGS) ../../$<
	mv obj/pdf/$*.pdf $@

bin/%.dvi: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.eps
	@! test -d obj/dvi || rm -rf obj/dvi
	@mkdir -p obj/dvi bin
	cd obj/dvi && yes X | $(TEXI2DVI) $(TEXIFLAGS) ../../$<
	mv obj/dvi/$*.dvi $@

bin/%.ps: doc/info/%.texinfo doc/info/*.texinfo # obj/logo.eps
	@! test -d obj/ps || rm -rf obj/ps
	@mkdir -p obj/ps bin
	cd obj/ps && yes X | texi2pdf $(TEXIFLAGS) --ps ../../$<
	mv obj/ps/$*.ps $@

