# Variables
CC=afl-clang-lto
CXX=afl-clang-lto++
CFLAGS=-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LDFLAGS=-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LIBSPNG_DIR=libspng
INCLUDE_DIR=$(LIBSPNG_DIR)/spng
BUILD_DIR=build
LIBSPNG_LIB=$(BUILD_DIR)/libspng.a

# Targets
all: $(BUILD_DIR)/decode_dev_zero

# Build libspng
$(LIBSPNG_LIB):
	cd $(LIBSPNG_DIR) && mkdir -p build && cd build && CC=$(CC) CXX=$(CXX) cmake -DBUILD_SHARED_LIBS=OFF .. && make
	cp $(LIBSPNG_DIR)/build/libspng_static.a $(BUILD_DIR)/

# Build decode_dev_zero
$(BUILD_DIR)/decode_dev_zero: decode_dev_zero.c $(LIBSPNG_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(BUILD_DIR) -l:libspng_static.a  decode_dev_zero.c -o $(BUILD_DIR)/decode_dev_zero $(LDFLAGS)

# Fuzzing target
fuzz: $(BUILD_DIR)/decode_dev_zero
	afl-fuzz -i input_dir -o output_dir $(BUILD_DIR)/decode_dev_zero @@

# Clean target
clean:
	rm -rf $(BUILD_DIR) output_dir
	cd $(LIBSPNG_DIR) && rm -rf build

.PHONY: all fuzz clean