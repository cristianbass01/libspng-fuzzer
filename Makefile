MAKE=make
CMAKE=cmake
CC=gcc

.PHONY: clean libspng fuzz run_fuzz_%

all: libspng fuzz

libspng/build/Makefile:
	$(CMAKE) -B libspng/build -S libspng

libspng: libspng/build/libspng.so

libspng/build/libspng.so: libspng/build/Makefile
	$(MAKE) -C libspng/build

fuzz: libspng/build/libspng.so fuzz/test.fuzz

# Maybe try with some fsanitize options: https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
fuzz/%.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CC) -o $@ $< -I libspng/spng -L libspng/build -lspng -g

# Usage: if you want to run the executable <FILE>.fuzz file corresponding
# to the source file <FILE>.c, you can run `make run_fuzz_<FILE>`
run_fuzz_%: fuzz/%.fuzz
	@LD_LIBRARY_PATH=libspng/build ./$<

clean:
	$(MAKE) -C libspng/build clean || true
	rm -rf libspng/build
	rm -rf fuzz/*.fuzz