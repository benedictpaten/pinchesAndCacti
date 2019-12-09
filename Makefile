rootPath = ./
include ./include.mk

libSources = impl/*.c
libHeaders = inc/*.h
libTests = tests/*.c
testBin = tests/testBin

all : externalToolsM ${libPath}/stPinchesAndCacti.a ${binPath}/stPinchesAndCactiTests

externalToolsM : 
	cd externalTools && $(MAKE) all

${libPath}/stPinchesAndCacti.a : ${libSources} ${libHeaders} ${basicLibsDependencies} externalToolsM
	${cxx} $(CPPFLAGS) ${cflags} $(CFLAGS) -I inc -I ${libPath}/ -c ${libSources}
	ar rc stPinchesAndCacti.a *.o
	ranlib stPinchesAndCacti.a 
	rm *.o
	mv stPinchesAndCacti.a ${libPath}/
	cp ${libHeaders} ${libPath}/

${binPath}/stPinchesAndCactiTests : ${libTests} ${libSources} ${libHeaders} ${basicLibsDependencies} externalToolsM
	${cxx} $(CPPFLAGS) ${cflags} $(CFLAGS) $(LDFLAGS) -I inc -I impl -I${libPath} -o ${binPath}/stPinchesAndCactiTests ${libTests} ${libSources} ${basicLibs}  ${libPath}/3EdgeConnected.a

clean : 
	cd externalTools && $(MAKE) clean
	rm -f *.o
	rm -f ${libPath}/stPinchesAndCacti.a ${binPath}/stPinchesAndCactiTests

test : all
	${binPath}/stPinchesAndCactiTests
