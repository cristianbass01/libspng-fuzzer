# Compilers
CC=gcc
CLANG=clang
AFLCC=afl-clang-lto
AFLCXX=afl-clang-lto++

# Libspng directories
LIBSPNG_DIR=libspng
INCLUDE_DIR=$(LIBSPNG_DIR)/spng
BUILD_LIBSPNG_DIR=$(LIBSPNG_DIR)/build

# Build directories
BUILD_DIR=build
LIBSPNG_LIB=$(BUILD_DIR)/libspng.a

# Flags
CFLAGS= -Wall -Wextra -Werror -fno-omit-frame-pointer -I $(INCLUDE_DIR) -L $(BUILD_LIBSPNG_DIR) -lspng -g $(CPPFLAGS) 
ASANFLAGS=-fsanitize=address 
MSANFLAGS=-fsanitize=memory -fPIE -pie -g

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
	$(AFLCC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(BUILD_DIR) -l:libspng_static.a  decode_dev_zero.c -o $(BUILD_DIR)/decode_dev_zero

# Fuzzing target
afl-fuzz: $(BUILD_DIR)/decode_dev_zero
	afl-fuzz -i input_dir -o output_dir $(BUILD_DIR)/decode_dev_zero @@



libspng/build/Makefile:
	cmake -B libspng/build -S libspng

libspng: libspng/build/libspng.so

libspng/build/libspng.so: libspng/build/Makefile
	make -C libspng/build

fuzz: libspng/build/libspng.so fuzz/test_fuzzer_descriptor.fuzz fuzz/decode_dev_zero.fuzz fuzz/simple_decode_dev_zero.fuzz fuzz/decode_encode_file.fuzz fuzz/generic_test.fuzz

# FUZZER BUILD
fuzz/%.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CC) -o $@ $< $(CFLAGS)

fuzz/%_asan.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CLANG) -o $@ $< $(CFLAGS) $(ASANFLAGS)

fuzz/%_msan.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CLANG) -o $@ $< $(CFLAGS) $(MSANFLAGS)


# Usage: if you want to run the executable <FILE>.fuzz file corresponding
# to the source file <FILE>.c, you can run `make run_fuzz_<FILE>`
run_fuzz_%: fuzz/%.fuzz
	@LD_LIBRARY_PATH=libspng/build ./$<

clean:
	$(MAKE) -C libspng/build clean || true
	rm -rf libspng/build
	rm -rf fuzz/*.fuzz
	rm -rf $(BUILD_DIR) output_dir
	rm -rf output/radamsa
	cd $(LIBSPNG_DIR) && rm -rf build