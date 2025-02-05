# Compilers
CC=gcc
CLANG=clang
AFLCC=afl-clang-fast
AFLCXX=afl-clang-fast++

# Libspng directories
LIBSPNG_DIR=libspng
INCLUDE_DIR=$(LIBSPNG_DIR)/spng
BUILD_LIBSPNG_DIR=$(LIBSPNG_DIR)/build

# Build directories
BUILD_DIR=build
LIBSPNG_LIB=$(BUILD_DIR)/libspng.a

# Flags
AFLCFLAGS= -Wall -Wextra -fno-omit-frame-pointer -I $(INCLUDE_DIR) -lz -lm
CFLAGS= -Wall -Wextra -fno-omit-frame-pointer -I $(INCLUDE_DIR) -L $(BUILD_LIBSPNG_DIR) -lspng -g $(CPPFLAGS) 
ASANFLAGS=-fsanitize=address
MSANFLAGS=-fsanitize=memory -fPIE -pie -g

# AFL++ Fuzzing input and minimization directories
IMAGE_DIR=images
UNIQUE_IMAGE_DIR=unique_images

# Targets
.PHONY: all clean libspng fuzz run_fuzz_% afl-fuzz

all: libspng fuzz #$(BUILD_DIR)/decode_dev_zero

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

libspng/build/Makefile:
	cmake -B libspng/build -S libspng

libspng: libspng/build/libspng.so

libspng/build/libspng.so: libspng/build/Makefile
	make -C libspng/build

generic_test: fuzz/generic_test.c libspng/build/libspng.so
	$(CC) -o $@ $< $(CFLAGS)

fuzz: fuzz/test_fuzzer_descriptor.fuzz fuzz/decode_dev_zero.fuzz fuzz/simple_decode_dev_zero.fuzz fuzz/decode_encode_file.fuzz fuzz/generic_test.fuzz \
	fuzz/afl_test_fuzzer_descriptor_nosan.fuzz fuzz/afl_decode_dev_zero_nosan.fuzz fuzz/afl_simple_decode_dev_zero_nosan.fuzz fuzz/afl_decode_encode_file_nosan.fuzz \
	fuzz/afl_generic_test_nosan.fuzz fuzz/afl_test_fuzzer_descriptor_asan.fuzz fuzz/afl_decode_dev_zero_asan.fuzz fuzz/afl_simple_decode_dev_zero_asan.fuzz \
	fuzz/afl_decode_encode_file_asan.fuzz fuzz/afl_generic_test_asan.fuzz fuzz/afl_test_fuzzer_descriptor_msan.fuzz fuzz/afl_decode_dev_zero_msan.fuzz \
	fuzz/afl_simple_decode_dev_zero_msan.fuzz fuzz/afl_decode_encode_file_msan.fuzz fuzz/afl_generic_test_msan.fuzz afl_minimize_input

# FUZZER BUILD
fuzz/%.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CC) -o $@ $< $(CFLAGS)

fuzz/%_asan.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CLANG) -o $@ $< $(CFLAGS) $(ASANFLAGS)

fuzz/%_msan.fuzz: fuzz/%.c libspng/build/libspng.so
	$(CLANG) -o $@ $< $(CFLAGS) $(MSANFLAGS)

# AFL BUILD

fuzz/afl_%_nosan.fuzz: fuzz/%.c libspng/spng/spng.c
	$(AFLCC) -static -o $@ fuzz/generic_test.c libspng/spng/spng.c $(AFLCFLAGS)

fuzz/afl_%_asan.fuzz: fuzz/%.c libspng/spng/spng.c
	AFL_USE_ASAN=1 $(AFLCC) -o $@ fuzz/generic_test.c libspng/spng/spng.c $(AFLCFLAGS)

fuzz/afl_%_msan.fuzz: fuzz/%.c libspng/spng/spng.c
	AFL_USE_MSAN=1 $(AFLCC) -o $@ fuzz/generic_test.c libspng/spng/spng.c $(AFLCFLAGS)

afl_minimize_input: fuzz/afl_generic_test_nosan.fuzz
	rm -rf $(UNIQUE_IMAGE_DIR)
	afl-cmin -T all -i $(IMAGE_DIR) -o $(UNIQUE_IMAGE_DIR) -- fuzz/afl_generic_test_nosan.fuzz @@


# Usage: if you want to run the executable <FILE>.fuzz file corresponding
# to the source file <FILE>.c, you can run `make run_fuzz_<FILE>`
run_fuzz_%: fuzz/%.fuzz
	@LD_LIBRARY_PATH=libspng/build ./$<

clean:
	$(MAKE) -C libspng/build clean || true
	rm -rf libspng/build
	rm -rf fuzz/*.fuzz
	rm -rf $(BUILD_DIR) output_dir
	rm -rf tmp
	cd $(LIBSPNG_DIR) && rm -rf build