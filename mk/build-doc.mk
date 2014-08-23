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


#obj/logo.svg: logo.svg
#	@mkdir -p obj
#	cp $< $@
#
#obj/logo.pdf: logo.svg
#	@mkdir -p obj
#	rsvg-convert --format=pdf $< > $@
#
#obj/logo.eps: obj/logo.ps
#	ps2eps $<
#
#obj/logo.ps: logo.svg
#	@mkdir -p obj
#	rsvg-convert --format=ps $< > $@


bin/%.info: doc/info/%.texinfo doc/info/*.texinfo
	@mkdir -p bin
	$(MAKEINFO) $(TEXIFLAGS) $<
	mv $*.info $@

bin/%.pdf: doc/info/%.texinfo doc/info/*.texinfo
	@mkdir -p obj bin
	cd obj && yes X | texi2pdf $(TEXIFLAGS) ../$<
	mv obj/$*.pdf $@

bin/%.dvi: doc/info/%.texinfo doc/info/*.texinfo
	@mkdir -p obj bin
	cd obj && yes X | $(TEXI2DVI) $(TEXIFLAGS) ../$<
	mv obj/$*.dvi $@

bin/%.ps: doc/info/%.texinfo doc/info/*.texinfo
	@mkdir -p obj bin
	cd obj && yes X | texi2pdf $(TEXIFLAGS) --ps ../$<
	mv obj/$*.ps $@

