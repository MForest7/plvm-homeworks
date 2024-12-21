CCFLAGS=-m32 -O2
CXXFLAGS=-m32 -O0 --std=c++20 -D_GLIBCXX_DEBUG -Iinclude
CXX=clang++

SRC=src
OBJ=build/src
BIN=build/bin
LIB=build/lib

all: interpreter bcdump bcstats

runtime.a: ../runtime/runtime.a
	cp $< $(OBJ)/$@

interpreter: interpreter.o interprete.o bytefile.o runtime.a verify.o
	cd $(OBJ) && $(CXX) $(CXXFLAGS) $^ -o ../bin/$@

bcdump: bcdump.o bytefile.o 
	cd $(OBJ) && $(CXX) $(CXXFLAGS) $^ -o ../bin/$@

bcstats: bcstats.o bytefile.o
	cd $(OBJ) && $(CXX) $(CXXFLAGS) $^ -o ../bin/$@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $(OBJ)/$@

clean:
	$(RM) build/*.a build/*.o build/bin/*~
