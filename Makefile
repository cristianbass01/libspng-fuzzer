MAKE=make
CMAKE=cmake
CC=gcc
AFLCC=afl-clang-lto
AFLCXX=afl-clang-lto++
CFLAGS=-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LDFLAGS=-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LIBSPNG_DIR=libspng
INCLUDE_DIR=$(LIBSPNG_DIR)/spng
BUILD_DIR=build
LIBSPNG_LIB=$(BUILD_DIR)/libspng.a

# Targets
.PHONY: all clean libspng fuzz run_fuzz_% afl-fuzz

all: libspng fuzz #$(BUILD_DIR)/decode_dev_zero


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build libspng
$(LIBSPNG_LIB): $(BUILD_DIR)
	cd $(LIBSPNG_DIR) && mkdir -p build && cd build && CC=$(AFLCC) CXX=$(AFLCXX) cmake -DBUILD_SHARED_LIBS=OFF .. && make
	cp $(LIBSPNG_DIR)/build/libspng_static.a $(BUILD_DIR)/

# Build decode_dev_zero
$(BUILD_DIR)/decode_dev_zero: fuzz/decode_dev_zero.c $(LIBSPNG_LIB)
	mkdir -p $(BUILD_DIR)
	$(AFLCC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(BUILD_DIR) -l:libspng_static.a  decode_dev_zero.c -o $(BUILD_DIR)/decode_dev_zero $(LDFLAGS)

# Fuzzing target
afl-fuzz: $(BUILD_DIR)/decode_dev_zero
	afl-fuzz -i input_dir -o output_dir $(BUILD_DIR)/decode_dev_zero @@




libspng/build/Makefile:
	$(CMAKE) -B libspng/build -S libspng

libspng: libspng/build/libspng.so

libspng/build/libspng.so: libspng/build/Makefile
	$(MAKE) -C libspng/build

fuzz: libspng/build/libspng.so fuzz/test_fuzzer_descriptor.fuzz fuzz/decode_dev_zero.fuzz fuzz/simple_decode_dev_zero.fuzz fuzz/decode_encode.fuzz

# Maybe try with some fsanitize options: https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
fuzz/%.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CC) -o $@ $< -I libspng/spng -L libspng/build -lspng -g $(CPPFLAGS) 

# Usage: if you want to run the executable <FILE>.fuzz file corresponding
# to the source file <FILE>.c, you can run `make run_fuzz_<FILE>`
run_fuzz_%: fuzz/%.fuzz
	@LD_LIBRARY_PATH=libspng/build ./$<

clean:
	$(MAKE) -C libspng/build clean || true
	rm -rf libspng/build
	rm -rf fuzz/*.fuzz
	rm -rf $(BUILD_DIR) output_dir
	cd $(LIBSPNG_DIR) && rm -rf build