AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = \
  -I$(includedir) \
  -isystem$(OFFLINE_MAIN)/include \
  -isystem$(ROOTSYS)/include

AM_LDFLAGS = \
  -L$(libdir) \
  -L$(OFFLINE_MAIN)/lib \
  -L$(OFFLINE_MAIN)/lib64 \
  -L$(ROOTSYS)/lib

pkginclude_HEADERS = \
  AnaTreev1.h \
  EventCut.h \
  EventCutReport.h \
  EventSelector.h \
  HijingTowerLoader.h \
  MinBiasCut.h

lib_LTLIBRARIES = \
  libmyana.la

libmyana_la_SOURCES = \
  AnaTreev1.cc \
  EventCutReport.cc \
  EventSelector.cc \
  HijingTowerLoader.cc \
  MinBiasCut.cc

libmyana_la_LIBADD = \
  -lfun4all \
  -lphool \
  -lffaobjects \
  -lffarawobjects \
  -lcalo_io \
  -lcalotrigger_io \
  -lcentrality_io \
  -ljetbase \
  -ljetbackground_io \
  -lglobalvertex_io \
  -lmbd_io \
  -lphhepmc_io \
  -lSubsysReco


BUILT_SOURCES = testexternals.cc

noinst_PROGRAMS = \
  testexternals

testexternals_SOURCES = testexternals.cc
testexternals_LDADD   = libmyana.la

testexternals.cc:
	echo "//*** this is a generated file. Do not commit, do not edit" > $@
	echo "int main()" >> $@
	echo "{" >> $@
	echo "  return 0;" >> $@
	echo "}" >> $@

clean-local:
	rm -f $(BUILT_SOURCES)
