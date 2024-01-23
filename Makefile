EXTRA_CXXFLAGS=
CXXFLAGS=-O3 -Wall -std=c++20 $(EXTRA_CXXFLAGS)

all: arith_compress arith_decompress

clean:
	rm -f arith_compress arith_decompress *.o
