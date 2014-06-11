rootPath = ..
include ../include.mk
# order is important, libraries first
modules = threeEdgeConnected
# We only use a few files from spimap to avoid the GSL dependency
SPIMAPSRCS = spimap/src/Tree.cpp spimap/src/phylogeny.cpp spimap/src/Sequences.cpp spimap/src/parsing.cpp spimap/src/logging.cpp spimap/src/seq.cpp
SPIMAPOBJS = $(SPIMAPSRCS:.cpp=.o)
#pulldown
.PHONY: all %.all clean %.clean

all : ${modules:%=all.%} ${libPath}/spimapRecon.a

all.%:
	cd $* && ${MAKE} all

$(SPIMAPOBJS): %.o: %.cpp
# UNDEBUG is required!!
	${cpp} -c $(cppflags) -UNDEBUG -o $@ $<

${libPath}/spimapRecon.a: $(SPIMAPOBJS) $(SPIMAPSRCS)
	ar rc spimapRecon.a $(SPIMAPOBJS)
	ranlib spimapRecon.a
	rm $(SPIMAPOBJS)
	mv spimapRecon.a ${libPath}

clean:  ${modules:%=clean.%}
	rm -rf lib/*.h bin/*.dSYM
	rm ${libPath}/spimapRecon.a

clean.%:
	cd $* && ${MAKE} clean

