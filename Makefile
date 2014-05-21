rootPath = ./
include ./include.mk

libSources = impl/*.c
libHeaders = inc/*.h
libTests = tests/*.c
testBin = tests/testBin
quickTreeObjects = externalTools/quicktree_1.1/obj/buildtree.o externalTools/quicktree_1.1/obj/cluster.o externalTools/quicktree_1.1/obj/distancemat.o externalTools/quicktree_1.1/obj/options.o externalTools/quicktree_1.1/obj/sequence.o externalTools/quicktree_1.1/obj/tree.o externalTools/quicktree_1.1/obj/util.o

all : externalToolsM ${libPath}/stPinchesAndCacti.a ${binPath}/stPinchesAndCactiTests

externalToolsM : 
	cd externalTools && make all

${libPath}/stPinchesAndCacti.a : ${libSources} ${libHeaders} externalToolsM ${basicLibsDependencies}
	${cxx} ${cflags} -I inc -I externalTools -I ${libPath}/ -c ${libSources}
	ar rc stPinchesAndCacti.a *.o ${quickTreeObjects}
	ranlib stPinchesAndCacti.a 
	rm *.o
	mv stPinchesAndCacti.a ${libPath}/
	cp ${libHeaders} ${libPath}/

${binPath}/stPinchesAndCactiTests : ${libTests} ${libSources} ${libHeaders} ${basicLibsDependencies} ${libPath}/3EdgeConnected.a
	${cxx} ${cflags} -I inc -I impl -I externalTools -I${libPath} -o ${binPath}/stPinchesAndCactiTests ${libTests} ${libSources} ${basicLibs}  ${libPath}/3EdgeConnected.a ${quickTreeObjects}

clean : 
	cd externalTools && make clean
	rm -f *.o
	rm -f ${libPath}/stPinchesAndCacti.a ${binPath}/stPinchesAndCactiTests

test : all
	python allTests.py
