#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <spng.h>
#include <unistd.h>
#include <fcntl.h>

// 0 for always read, 
// 1 for always write
// 2 for random read or write, 
#define TEST_TYPE 2

// 0 uses normal filename parsing function
// 1 uses AFL filename parsing function
#define AFL_MODE 0

// recommended maximum length for strings in initialization in write
#define RECOMMENDED_MAX_LENGTH 100

// probability of initializing the fields in write
#define INITIALIZATION_PROB 1.0

#define test(fn)                                                       \
    printf("Testing %s... ", #fn);                                     \
    fflush(stdout);                                                    \
    fflush(stderr);                                                    \
    fn_ret = fn;                                                       \
    if (fn_ret)                                                        \
        printf("returned %d: %s\n", fn_ret, spng_strerror(fn_ret));    \
    else printf("OK\n");


// DECLARATION:
int fuzz_spng_read(const uint8_t* data, size_t size);

enum feature {
    UNKNOWN,
    SIZE,
    BACKGROUND,
    TRANSPARENCY,
    GAMMA,
    FILTERING,
    PALETTE,
    CHUNKS,
    CHUNK_ORDERING,
    HISTOGRAM,
    PHYSICAL_PIXELS,
    TEXT,
    TIME,
    SIGNIFICANT_BITS,
    COMPRESSION,
    CORRUPTED,
    EXIF
};

enum background {
    NO_BACKGROUND,
    BLACK,
    GRAY,
    WHITE,
    YELLOW
};

enum transparency {
    NO_TRANSPARENT,
    TRANSPARENT
};

enum filtering {
    NO_FILTERING,
    SUB,
    UP,
    AVERAGE,
    PAETH
};

enum palette {
    PALETTE_CHUNK,
    SUGGESTED_PALETTE_1_BYTE,
    SUGGESTED_PALETTE_2_BYTE
};

enum text {
    NO_TEXT,
    TEXT_CHUNK,
    COMPRESSED_TEXT,
    INTERNATIONAL_TEXT_ENGLISH,
    INTERNATIONAL_TEXT_FINNISH,
    INTERNATIONAL_TEXT_GREEK,
    INTERNATIONAL_TEXT_HINDI,
    INTERNATIONAL_TEXT_JAPANESE,
};

// Define a struct to store PNG properties
typedef struct {
    enum feature testFeature;       // Feature being tested (e.g., gamma, filtering)
    int size;                       // Size of the image
    enum background background;     // Background color
    enum transparency transparency; // Transparency value
    double gamma;                      // Gamma value
    enum spng_filter filtering;       // Filtering method
    enum palette palette;           // Palette method
    int significant_bits;           // Significant bits
    int histogram_colors;           // Number of colors in the histogram
    int ppu_x;            // Physical pixels
    int ppu_y;            // Physical pixels
    int unit_specifier;   // Unit specifier
    enum text text;
    struct spng_time time;       // Time
    int chunk_ordering;             // Chunk ordering
    int compression;                // Compression level
    enum spng_interlace_method interlace; // Interlace method (enum)
    enum spng_color_type colorType;       // Color type (enum)
    uint8_t bitDepth;           // Bit depth of the image
} PNGConfig;


int fuzz_spng_write(const uint8_t* data, size_t size, PNGConfig config);

PNGConfig get_PNGConfig(const char *fileName);
void get_file_code(const char *path, char *output);
void get_file_code_afl(const char *path, char *output);
////////////////////////////////////////
// MAIN:
////////////////////////////////////////

int main(int argc, char **argv)
{
    
 
    // Possibly we need to delete this part because the seed is set to the value at middle of the buffer
    /*
    int urandom = open("/dev/urandom", O_RDONLY);
    
    unsigned int seed;
    if(read(urandom, &seed, sizeof(seed)) != sizeof(seed))
    {
        fprintf(stderr, "error reading from /dev/urandom\n");
        srand(time(NULL));
    }
    else
    {
        srand(seed);
    }
    close(urandom);
    */
    char *buf = NULL;
    long siz_buf;
    int fd = -1;

    if(argc < 2)
    {
        fprintf(stderr, "no input file\n");
        goto error;
    }

    fd = open(argv[1], O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "error opening input file %s\n", argv[1]);
        goto error;
    }

    // get file size
    siz_buf = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if(siz_buf < 1) {
        fprintf(stderr, "file is empty\n");
        goto error;
    }

    // allocate buffer
    buf = (char*)malloc(siz_buf);
    if(buf == NULL)
    {
        fprintf(stderr, "malloc() failed\n");
        goto error;
    }

    // read file
    if(read(fd, buf, siz_buf) == -1)
    {
        fprintf(stderr, "fread() failed\n");
        goto error;
    }

    unsigned int seed;

    // Setting seed to random value in the middle of the buffer
    seed = (unsigned int) buf[siz_buf/2];
    srand(seed);

    int success = 0;
    char fileName[256];

#if AFL_MODE == 1
    get_file_code_afl(argv[1], fileName);
#else
    get_file_code(argv[1], fileName);
#endif
    printf("File name: %s\n", fileName);

#if TEST_TYPE == 0 // Specific read
    success = fuzz_spng_read((const uint8_t *)buf, siz_buf);
#elif TEST_TYPE == 1 // Specific write
    PNGConfig config = getPNGConfig(fileName);
    success = fuzz_spng_write((const uint8_t *)buf, siz_buf, config);
#else // Random read or write
    if (rand() % 2 == 0)
        success = fuzz_spng_read((const uint8_t *)buf, siz_buf);
    else{
        PNGConfig config = get_PNGConfig(fileName);
        success = fuzz_spng_write((const uint8_t *)buf, siz_buf, config);
    }
#endif

    free(buf);
    if (fd != -1) 
        close(fd);

    return success;

error:
    free(buf);
    if (fd != -1) 
        close(fd);

    return 0;
}

//////////////////////////
// DEFINITIONS:
//////////////////////////

// FUNCTIONS TAKEN FROM spng.c

/// @brief Struct to store buffer state
struct buf_state
{
    const uint8_t *data;
    size_t bytes_left;
};

/// @brief Read function for spng_set_png_stream
/// @param ctx - spng context
/// @param user - user data
/// @param dest - destination buffer
/// @param length - length of data to read
/// @return - 0 on success, SPNG_IO_EOF on failure
static int buffer_read_fn(spng_ctx *ctx, void *user, void *dest, size_t length)
{
    struct buf_state *state = (struct buf_state*)user;
    (void)ctx;

    if(length > state->bytes_left) return SPNG_IO_EOF;

    memcpy(dest, state->data, length);

    state->bytes_left -= length;
    state->data += length;

    return 0;
}

/// @brief Write function for spng_set_png_stream
/// @param ctx - spng context
/// @param user - user data
/// @param data - data to write
/// @param length - length of data to write
/// @return - 0 on success, SPNG_IO_EOF on failure
static int stream_write_fn(spng_ctx *ctx, void *user, void *data, size_t length)
{
    struct buf_state *state = (struct buf_state*)user;
    (void)ctx;

    if(length > state->bytes_left) return SPNG_IO_EOF;

    state->bytes_left -= length;

    if(state->data)
    {
        memcpy(data, state->data, length);
        state->data += length;
    }

    return 0;
}

// end spng.c

/////////////////////////////////////////////
// HELP FUNCTIONS:
/////////////////////////////////////////////

/// @brief Helper function to choose random options for spng_set_option
/// @param options_list - list of options to choose from
/// @param TOTAL_OPTIONS - total number of options in options_list
/// @param num_options - number of options to choose
/// @param chosen_options - array to store chosen options
/// @param chosen_values - array to store chosen values
void choose_random_options(enum spng_option options_list[], int TOTAL_OPTIONS, int num_options, int chosen_options[], int chosen_values[]){
    for(int i = 0; i < num_options; i++){
        chosen_options[i] = options_list[rand() % TOTAL_OPTIONS];
        int compression_levels[] = {0, 1, 2, 9};
        int filter_choices[] = {SPNG_DISABLE_FILTERING, SPNG_FILTER_CHOICE_NONE, SPNG_FILTER_CHOICE_SUB, SPNG_FILTER_CHOICE_UP, SPNG_FILTER_CHOICE_AVG, SPNG_FILTER_CHOICE_PAETH, SPNG_FILTER_CHOICE_ALL};
        switch (chosen_options[i])
        {
        case SPNG_KEEP_UNKNOWN_CHUNKS:
            chosen_values[i] = rand() % 2;
            break;
        case SPNG_IMG_COMPRESSION_LEVEL:
            chosen_values[i] = compression_levels[rand() % 4];
            break;
        case SPNG_IMG_WINDOW_BITS:
            chosen_values[i] = rand() % 8 + 8;
            break;
        case SPNG_IMG_MEM_LEVEL:
            chosen_values[i] = rand() % 9 + 1;
            break;
        case SPNG_IMG_COMPRESSION_STRATEGY:
            chosen_values[i] = rand() % 5;
            break;
        case SPNG_TEXT_COMPRESSION_LEVEL:
            chosen_values[i] = compression_levels[rand() % 4];
            break;
        case SPNG_TEXT_WINDOW_BITS:
            chosen_values[i] = rand() % 8 + 8;
            break;
        case SPNG_TEXT_MEM_LEVEL:
            chosen_values[i] = rand() % 9 + 1;
            break;
        case SPNG_TEXT_COMPRESSION_STRATEGY:
            chosen_values[i] = rand() % 5;
            break;
        case SPNG_FILTER_CHOICE:
            chosen_values[i] = filter_choices[rand() % 7];
            break;
        case SPNG_CHUNK_COUNT_LIMIT:
            chosen_values[i] = rand() % UINT32_MAX;
            break;
        case SPNG_ENCODE_TO_BUFFER:
            chosen_values[i] = rand() % 2;
            break;
        default:
            break;
        }
    }
}

/////////////////////////////////////////////
// FUZZ FUNCTIONS:
/////////////////////////////////////////////

/// @brief Fuzz function for spng_read
/// @param data - data to read
/// @param size - size of data
/// @return - 0 on success, 1 on failure
int fuzz_spng_read(const uint8_t* data, size_t size)
{
    printf("Fuzzing spng_read...\n");

    // Initialization
    int fn_ret;
    unsigned char *img = NULL;
    FILE *file = NULL;

    struct spng_ihdr ihdr;
    struct spng_plte plte;
    struct spng_trns trns;

    struct spng_chrm chrm;

    struct spng_chrm_int chrm_int;

    double gama;
    uint32_t gama_int;
    struct spng_iccp iccp;
    struct spng_sbit sbit;

    uint8_t srgb_rendering_intent;

    struct spng_text text[4] = {0};
    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_phys phys;
    struct spng_splt splt[4] = {0};
    struct spng_time time;
    struct spng_unknown_chunk chunks[4] = {0};
    uint32_t n_text = 4, n_splt = 4, n_chunks = 4;

    struct spng_offs offs;
    struct spng_exif exif;

    struct buf_state state;
    state.data = data;
    state.bytes_left = size;

    // Initialize enums from spng.h
    enum spng_format fmt_flags[] = {
        SPNG_FMT_RGBA8, SPNG_FMT_RGBA16, SPNG_FMT_RGB8,
        SPNG_FMT_GA8, SPNG_FMT_GA16, SPNG_FMT_G8,
        SPNG_FMT_PNG, SPNG_FMT_RAW
    };
    int TOTAL_TMP_FLAGS = sizeof(fmt_flags) / sizeof(enum spng_format);

    enum spng_decode_flags decode_flags[] = {
        SPNG_DECODE_USE_TRNS, SPNG_DECODE_USE_GAMA, SPNG_DECODE_USE_SBIT,
        SPNG_DECODE_TRNS, SPNG_DECODE_GAMMA, SPNG_DECODE_PROGRESSIVE
    };
    int TOTAL_DECODE_FLAGS = sizeof(decode_flags) / sizeof(enum spng_decode_flags);

    enum spng_option options_list[] = {
        SPNG_KEEP_UNKNOWN_CHUNKS, // true, false
        SPNG_IMG_COMPRESSION_LEVEL, // defaul -1, 0-9
        SPNG_IMG_WINDOW_BITS, // default 15, 8-15
        SPNG_IMG_MEM_LEVEL, // default 8, 1-9
        SPNG_IMG_COMPRESSION_STRATEGY, // default 0, 0-4
        SPNG_TEXT_COMPRESSION_LEVEL, // default -1, 0-9
        SPNG_TEXT_WINDOW_BITS, // default 15, 8-15
        SPNG_TEXT_MEM_LEVEL, // default 8, 1-9
        SPNG_TEXT_COMPRESSION_STRATEGY, // default 0, 0-4
        SPNG_FILTER_CHOICE, // default 0, 0-4
        SPNG_CHUNK_COUNT_LIMIT, // default 0, 0-UINT32_MAX
        SPNG_ENCODE_TO_BUFFER, // true, false
    };
    int TOTAL_OPTIONS = sizeof(options_list) / sizeof(enum spng_option);

    // Set up random configuration
    int stream = rand() % 2;
    int file_stream = rand() % 2;
    int discard = rand() % 2;
    int progressive = rand() % 2;
    int fmt = fmt_flags[rand() % TOTAL_TMP_FLAGS];
    fmt = SPNG_FMT_RGBA8;
    int flags = decode_flags[rand() % TOTAL_DECODE_FLAGS];
    
    int num_options = rand() % TOTAL_OPTIONS;
    int chosen_options[num_options+1];
    int chosen_values[num_options+1];
    choose_random_options(options_list, TOTAL_OPTIONS, num_options, chosen_options, chosen_values);

    // Print configuration
    printf("Configuration:\n");
    printf(" - stream: %d\n", stream);
    printf(" - file_stream: %d\n", file_stream);
    printf(" - discard: %d\n", discard);
    printf(" - progressive: %d\n", progressive);
    printf(" - fmt: %d\n", fmt);
    printf(" - flags: %d\n", flags);
    printf(" - num_options: %d\n", num_options);

    for(int i = 0; i < num_options; i++){
        printf(" - Option %d, Value %d\n", chosen_options[i], chosen_values[i]);
    }
    
    // end Initialization

    // print version
    printf("libspng version: %s\n", spng_version_string());

    // Test spng_ctx_new
    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_IGNORE_ADLER32);
    if(ctx == NULL) return 0;

    if(stream)
    {
        if(file_stream) {
            // Simulate a file stream from data
            file = fmemopen((void*)data, size, "rb");

            if(file == NULL) goto err;
            
            test(spng_set_png_file(ctx, file));
        }
        else {
            test(spng_set_png_stream(ctx, buffer_read_fn, &state));
        }
    }
    else{
        test(spng_set_png_buffer(ctx, (void*)data, size));
    }

    if(fn_ret) goto err;

    uint32_t width, height;
    test(spng_get_image_limits(ctx, &width, &height));
    test(spng_set_image_limits(ctx, 200000, 200000));

    size_t limits;
    test(spng_get_chunk_limits(ctx, &limits, &limits));

    limits = 4 * 1000 * 1000;
    test(spng_set_chunk_limits(ctx, limits, limits * 2));

    test(spng_set_crc_action(ctx, SPNG_CRC_USE, discard ? SPNG_CRC_DISCARD : SPNG_CRC_USE))

    // Test set_option with different configurations
    for(int i = 0; i < num_options; i++){
        test(spng_set_option(ctx, chosen_options[i], chosen_values[i]));
    }

    test(spng_set_option(ctx, SPNG_KEEP_UNKNOWN_CHUNKS, 1));

    // Test get_option for all options
    int value;
    for(int i = 0; i < TOTAL_OPTIONS; i++){
        test(spng_get_option(ctx, options_list[i], &value));
    }

    size_t out_size = 0;
    test(spng_decoded_image_size(ctx, fmt, &out_size));
    if(fn_ret) goto err;
    if(out_size > 80000000) goto err;

    img = (unsigned char*)malloc(out_size);
    if(img == NULL) goto err;

    //// Test get methods
    test(spng_get_ihdr(ctx, &ihdr));
    // ERROR IN MEMORY SANITIZER FOR GET METHODS
    test(spng_get_plte(ctx, &plte));
    test(spng_get_trns(ctx, &trns));
    test(spng_get_chrm(ctx, &chrm));
    test(spng_get_chrm_int(ctx, &chrm_int));
    test(spng_get_gama(ctx, &gama));
    test(spng_get_gama_int(ctx, &gama_int));
    test(spng_get_iccp(ctx, &iccp));
    test(spng_get_sbit(ctx, &sbit));
    test(spng_get_srgb(ctx, &srgb_rendering_intent));

    // Test spng_get_text for 4 and for arbitrary number
    printf("Testing spng_get_text...");
    if(!spng_get_text(ctx, text, &n_text))
    {/* Up to 4 entries were read, get the actual count */
        spng_get_text(ctx, NULL, &n_text);
        for(uint32_t i = 0; i < n_text; i++)
        {
            // All strings should be non-NULL
            if(text[i].text == NULL ||
                text[i].keyword[0] == '\0' ||
                text[i].language_tag == NULL ||
                text[i].translated_keyword == NULL ||
                memchr(text[i].keyword, 0, 80) == NULL)
            {
                spng_ctx_free(ctx);
                free(img);
                return 1;
            }

            /* This shouldn't cause issues either */
            text[i].length = strlen(text[i].text);
        }
    }
    printf("OK\n");

    test(spng_get_bkgd(ctx, &bkgd));
    test(spng_get_hist(ctx, &hist));
    test(spng_get_phys(ctx, &phys));

    // Test spng_get_splt for 4 and for arbitrary number
    printf("Testing spng_get_splt...");
    if(!spng_get_splt(ctx, splt, &n_splt))
    {/* Up to 4 entries were read, get the actual count */
        spng_get_splt(ctx, NULL, &n_splt);
        for(uint32_t i = 0; i < n_splt; i++)
        {
            if(splt[i].name[0] == '\0' ||
                splt[i].entries == NULL ||
                memchr(splt[i].name, 0, 80) == NULL)
            {
                spng_ctx_free(ctx);
                free(img);
                return 1;
            }
        }
    }
    printf("OK\n");

    // Test spng_get_unknown_chunks for 4 and for arbitrary number
    if(!spng_get_unknown_chunks(ctx, chunks, &n_chunks))
    {
        spng_get_unknown_chunks(ctx, NULL, &n_chunks);
        for(uint32_t i = 0; i < n_chunks; i++)
        {
            if( (chunks[i].length && !chunks[i].data) ||
                (!chunks[i].length && chunks[i].data) )
            {
                spng_ctx_free(ctx);
                free(img);
                return 1;
            }
        }
    }

    test(spng_get_offs(ctx, &offs));
    test(spng_get_exif(ctx, &exif));

    if(progressive)
    {
        // test scanline
        test(spng_decode_scanline(ctx, img, out_size));

        // test decode_chunks
        test(spng_decode_chunks(ctx));

        // test decode_image
        test(spng_decode_image(ctx, NULL, 0, fmt, flags | SPNG_DECODE_PROGRESSIVE));
        if(fn_ret) goto err;

        // test row
        size_t ioffset, out_width = out_size / ihdr.height;
        struct spng_row_info ri;
        do
        {
            if(spng_get_row_info(ctx, &ri)) break;
            ioffset = ri.row_num * out_width;
        }while(!spng_decode_row(ctx, img + ioffset, out_size));
    }
    else{
        test(spng_decode_image(ctx, img, out_size, fmt, flags));
        if(fn_ret) goto err;
    }

    test(spng_get_time(ctx, &time));

    // Test spng_ctx_free
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 
    // end spng_ctx_free

    printf("Finished\n");
    if(img != NULL) free(img);
    if(file != NULL) fclose(file);

    return 0;

err:
    // Test spng_ctx_free
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 
    // end spng_ctx_free

    printf("Finished with error\n");
    if(img != NULL) free(img);
    if(file != NULL) fclose(file);

    return 0;
}

//////////////////////////////////////////////
// CONFIGURATION FUNCTIONS:
//////////////////////////////////////////////

enum background getBackground(char background) {
    switch (background)
    {
    case 'a':
        return NO_BACKGROUND;
    case 'b':
        return BLACK;
    case 'g':
        return GRAY;
    case 'w':
        return WHITE;
    case 'y':
        return YELLOW;
    default:
        return NO_BACKGROUND;
    }
}

// Function to derive a PNGConfig from the file name
PNGConfig get_PNGConfig(const char *fileName) {
    PNGConfig config;

    // Default configuration
    config.testFeature = UNKNOWN;
    config.size = 32;
    config.background = NO_BACKGROUND;
    config.transparency = NO_TRANSPARENT;
    config.gamma = 1.0;
    config.filtering = 0;
    config.palette = PALETTE_CHUNK;
    config.significant_bits = 0;
    config.histogram_colors = 0;
    config.ppu_x = 0;
    config.ppu_y = 0;
    config.unit_specifier = 0;
    config.text = NO_TEXT;
    config.chunk_ordering = 0;
    config.compression = 0;
    config.interlace = SPNG_INTERLACE_NONE;
    config.colorType = SPNG_COLOR_TYPE_TRUECOLOR;
    config.bitDepth = 8;
    config.time.day = 0;
    config.time.month = 0;
    config.time.year = 0;
    config.time.hour = 0;
    config.time.minute = 0;
    config.time.second = 0;

    if(strlen(fileName) < 8) return config;

    char parameter[3] = {fileName[1], fileName[2], '\0'};

    switch (fileName[0])
    {
    case 's': 
        // Odd sizes: from 1 to 9 and from 32 to 40
        config.testFeature = SIZE;
        config.size = atoi(parameter);
        break;
    
    case 'b': 
        // Background colors:
        // a: no background, b : black, g : gray, w : white, y : yellow
        config.testFeature = BACKGROUND;
        config.background = getBackground(fileName[1]);
        break;
    
    case 't':
        // Transparency:
        config.testFeature = TRANSPARENCY;

        if (fileName[1] == 'b') {
            config.transparency = TRANSPARENT;
            config.background = getBackground(fileName[2]);
        }
        else if (strcmp(parameter, "p0") == 0) {
            config.transparency = NO_TRANSPARENT;
        }
        else if (strcmp(parameter, "p1") == 0) {
            config.transparency = TRANSPARENT;
            config.background = NO_BACKGROUND;
        }
        else if(strcmp(parameter, "m3") == 0) {
            config.transparency = TRANSPARENT;
        }
        break;

    case 'g':
        // Gamma: 0.35, 0.45, 0.55, 0.70, 1.00, 2.50
        config.testFeature = GAMMA;
        config.gamma = atof(parameter) / 10;
        if (config.gamma <= 0.5)
            config.gamma += 0.05;
        break;

    case 'f':
        // Filtering: 0, 1, 2, 3, 4
        config.testFeature = FILTERING;
        config.filtering = atoi(parameter);
        if (config.filtering == 99)
            config.filtering = 0;
        break;

    case 'p':
        // Palette: 0, 1, 2
        config.testFeature = PALETTE;
        if (strcmp(parameter, "p0") == 0)
            config.palette = PALETTE_CHUNK;
        else if (strcmp(parameter, "s1") == 0)
            config.palette = SUGGESTED_PALETTE_1_BYTE;
        else if (strcmp(parameter, "s2") == 0)
            config.palette = SUGGESTED_PALETTE_2_BYTE;
        break;

    case 'c':
        // Chunks: THIS IS A MESS
        if (fileName[1] == 's') {
            config.testFeature = SIGNIFICANT_BITS;
            config.significant_bits = fileName[2] - '0';
        }
        else if (fileName[1] == 'h') {
            config.testFeature = HISTOGRAM;
            if (fileName[2] == '1') {
                config.histogram_colors = 15;
            }
            else {
                config.histogram_colors = 256;
            }
        }
        else if (fileName[1] == 'd') {
            config.testFeature = PHYSICAL_PIXELS;
            switch (fileName[2])
            {
            case 'f':
                config.ppu_x = 8;
                config.ppu_y = 32;
                break;
            case 'h':
                config.ppu_x = 32;
                config.ppu_y = 8;
                break;
            case 's':
                config.ppu_x = 8;
                config.ppu_y = 8;
                break;
            case 'u':
                config.ppu_x = rand() % 1000;
                config.ppu_y = rand() % 1000;
                config.unit_specifier = rand() % 2;
                break;
            default:
                break;
            }
        }
        else if (fileName[1] == 't') {
            config.testFeature = TEXT;
            switch (fileName[2])
            {
                case '0':
                    config.text = NO_TEXT;
                    break;
                case '1':
                    config.text = TEXT_CHUNK;
                    break;
                case 'z':
                    config.text = COMPRESSED_TEXT;
                    break;
                case 'e':
                    config.text = INTERNATIONAL_TEXT_ENGLISH;
                    break;
                case 'f':
                    config.text = INTERNATIONAL_TEXT_FINNISH;
                    break;
                case 'g':
                    config.text = INTERNATIONAL_TEXT_GREEK;
                    break;
                case 'h':
                    config.text = INTERNATIONAL_TEXT_HINDI;
                    break;
                case 'j':
                    config.text = INTERNATIONAL_TEXT_JAPANESE;
                    break;
                default:
                    config.text = NO_TEXT;
                    break;
            }
        }
        else if (fileName[1] == 'm') {
            config.testFeature = TIME;
            if (fileName[2] == '7') {
                config.time.day = 1;
                config.time.month = 1;
                config.time.year = 1970;
            }
            else if (fileName[2] == '9') {
                config.time.day = 31;
                config.time.month = 12;
                config.time.year = 1999;
            }
            else if (fileName[2] == '0') {
                config.time.day = 1;
                config.time.month = 1;
                config.time.year = 2000;
            }
        }
        break;

    case 'o':
        // chuck ordering
        config.testFeature = CHUNK_ORDERING;
        config.chunk_ordering = fileName[2] - '0';
        break;

    case 'z':
        // Compression: 0, 1, 2, 3, 4
        config.testFeature = COMPRESSION;
        config.compression = atoi(parameter);
        break;

    case 'x':
        // Corrupted: 0, 1, 2, 3, 4
        config.testFeature = CORRUPTED;
        break;

    case 'e':
        // EXIF
        config.testFeature = EXIF;
        break;

    default:
        break;
    }
    
    // Determine interlacing (fourth character)
    char interlacing = fileName[3];
    config.interlace = (interlacing == 'i') ? SPNG_INTERLACE_ADAM7 : SPNG_INTERLACE_NONE;

    // Extract color type (4 and 5 characters)
    // 4 character is the color type code
    // 5 character is the color type description
    char colorTypeCode[3] = {fileName[4], '\0'};
    config.colorType = atoi(colorTypeCode);

    // Extract bit depth (seventh character)
    char bitDepthCode[3] = {fileName[6], fileName[7], '\0'};
    config.bitDepth = atoi(bitDepthCode);

    return config;
}

enum spng_format get_fmt_from_config(const PNGConfig *config) {
    // Determine the format based on color type and bit depth
    if (config->colorType == SPNG_COLOR_TYPE_GRAYSCALE && config->bitDepth == 8)
        return SPNG_FMT_G8;
    else if (config->colorType == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA){
        if (config->bitDepth == 8)
            return SPNG_FMT_GA8;
        else
            return SPNG_FMT_GA16;
    }
    else if (config->colorType == SPNG_COLOR_TYPE_TRUECOLOR && config->bitDepth == 8)
        return SPNG_FMT_RGB8;
    else if (config->colorType == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA){
        if (config->bitDepth == 8)
            return SPNG_FMT_RGBA8;
        else
            return SPNG_FMT_RGBA16;
    }
    else if (config->colorType == SPNG_COLOR_TYPE_INDEXED)
        return SPNG_FMT_PNG;
    else
        return SPNG_FMT_RAW;
}

enum spng_decode_flags get_decode_flags_from_config(const PNGConfig *config) {
    int flags = 0;
    if (config->testFeature == TRANSPARENCY)
        flags |= SPNG_DECODE_TRNS;
    if (config->testFeature == GAMMA)
        flags |= SPNG_DECODE_GAMMA;

    if (config->testFeature == SIGNIFICANT_BITS) {
        flags |= SPNG_DECODE_USE_SBIT;  // Enable using the sBIT chunk during decoding
    }
    return flags;
}

void get_file_code_afl(const char *path, char *output) {
    
    printf("Reading file code from AFL path\n");
    const char *lastSlash = strrchr(path, '/'); // For UNIX-like paths
    if (!lastSlash) {
        lastSlash = strrchr(path, '\\'); // For Windows paths
    }
    const char *fileName = lastSlash ? lastSlash + 1 : path; // Start after the slash

    int comma_counter = 0;

    //size_t intermediate_length = strlen(fileName);

    while(comma_counter < 3 && *fileName!='\0'){ //Reaching last comma of afl filenames
        if(*fileName == ','){
            comma_counter++;
        }
        fileName++;
    }

    fileName += 5; //skiping the "orig:" part of the string
    size_t length = strlen(fileName) < 8 ? strlen(fileName) : 8;

    memcpy(output, fileName, length); // Copy the filename part
    output[length] = '\0'; // Null-terminate the string
}

void get_file_code(const char *path, char *output) {
    const char *lastSlash = strrchr(path, '/'); // For UNIX-like paths
    if (!lastSlash) {
        lastSlash = strrchr(path, '\\'); // For Windows paths
    }
    const char *fileName = lastSlash ? lastSlash + 1 : path; // Start after the slash
    size_t length = strlen(fileName) < 8 ? strlen(fileName) : 8;

    memcpy(output, fileName, length); // Copy the filename part
    output[length] = '\0'; // Null-terminate the string
}

char* get_random_string(size_t length) {
    if (length == 0) return NULL;

    char *str = (char *)malloc(length + 1);
    if (!str) return NULL;

    for (size_t i = 0; i < length; i++) {
        str[i] = rand() % 256;
    }
    str[length] = '\0';
    return str;
}

int random_choice(){
    return rand() % 2;
}

int select_random_with_probability(double success_rate){
    return rand() % 100 < (success_rate * 100);
}

int choose_to_initialize(){
    // 10% probability of not initializing
    return select_random_with_probability(INITIALIZATION_PROB);
}

/// @brief Fuzz function for spng_write
/// @param data - data to write
/// @param size - size of data
/// @return - 0 on success, 1 on failure
int fuzz_spng_write(const uint8_t* data, size_t size, PNGConfig config)
{
    printf("Fuzzing spng_write...\n");

    // Initialization
    int fn_ret;
    unsigned char *img = NULL;
    size_t img_size;
    size_t pixel_bits = 0;

    void *png = NULL;
    size_t png_size = 0;

    struct spng_ihdr ihdr;
    if (choose_to_initialize()){
        if (select_random_with_probability(0.9)){
            ihdr.width = config.size;
            ihdr.height = config.size;
            ihdr.bit_depth = config.bitDepth;
            ihdr.color_type = config.colorType;
            ihdr.compression_method = config.compression;
            ihdr.filter_method = config.filtering;
            ihdr.interlace_method = config.interlace;
        }
        else {
            ihdr.width = rand() % UINT32_MAX;
            ihdr.height = rand() % UINT32_MAX;
            ihdr.bit_depth = rand() % UINT8_MAX;
            ihdr.color_type = rand() % 7;
            ihdr.compression_method = rand() % 10;
            ihdr.filter_method = rand() % 10;
            ihdr.interlace_method = rand() % 2;
        }
    }

    struct spng_plte plte;
    if (choose_to_initialize()){
        plte.n_entries = rand() % RECOMMENDED_MAX_LENGTH;
        for (uint32_t i = 0; i < plte.n_entries; i++) {
            plte.entries[i].red = rand() % UINT8_MAX;
            plte.entries[i].green = rand() % UINT8_MAX;
            plte.entries[i].blue = rand() % UINT8_MAX;
            plte.entries[i].alpha = rand() % UINT8_MAX;
        }
    }
    

    struct spng_trns trns;
    if (choose_to_initialize()){
        trns.gray = rand() % UINT16_MAX;
        trns.red = rand() % UINT16_MAX;
        trns.green = rand() % UINT16_MAX;
        trns.blue = rand() % UINT16_MAX;
        trns.n_type3_entries = rand() % UINT32_MAX;
        int entries = rand() % 256;
        for (int i = 0; i < entries; i++) {
            trns.type3_alpha[i] = rand() % UINT8_MAX;
        }
    }
    

    struct spng_chrm chrm;
    if (choose_to_initialize()){
        // random double values
        chrm.white_point_x = rand() % 65536 / 1000.0;
        chrm.white_point_y = rand() % 65536 / 1000.0;
        chrm.red_x = rand() % 65536 / 1000.0;
        chrm.red_y = rand() % 65536 / 1000.0;
        chrm.green_x = rand() % 65536 / 1000.0;
        chrm.green_y = rand() % 65536 / 1000.0;
        chrm.blue_x = rand() % 65536 / 1000.0;
        chrm.blue_y = rand() % 65536 / 1000.0;
    }
    

    struct spng_chrm_int chrm_int;
    // random uint32_t values
    if(choose_to_initialize()){
        chrm_int.white_point_x = rand() % UINT32_MAX;
        chrm_int.white_point_y = rand() % UINT32_MAX;
        chrm_int.red_x = rand() % UINT32_MAX;
        chrm_int.red_y = rand() % UINT32_MAX;
        chrm_int.green_x = rand() % UINT32_MAX;
        chrm_int.green_y = rand() % UINT32_MAX;
        chrm_int.blue_x = rand() % UINT32_MAX;
        chrm_int.blue_y = rand() % UINT32_MAX;
    }
    

    double gama;
    uint32_t gama_int;

    if (config.testFeature == GAMMA) {
        gama = config.gamma;
        gama_int = (uint32_t)(config.gamma * 100);
    }
    else {
        gama = rand() % 65536 / 10000.0;
        gama_int = rand() % UINT32_MAX;
    }
    
    

    struct spng_iccp iccp;
    // Initialize or not at random (more probble to initialize)
    if (choose_to_initialize()){
        int length = rand() % 80;
        for (int i = 0; i < length; i++) {
            iccp.profile_name[i] = rand() % 256;
        }
        iccp.profile_name[length] = '\0';
        iccp.profile = get_random_string(rand() % RECOMMENDED_MAX_LENGTH); // random string (at most 1000 chars)
        iccp.profile_len = rand() % RECOMMENDED_MAX_LENGTH; // random length that can be larger than the actual string
    }

    struct spng_sbit sbit;
    if(choose_to_initialize()){
        if (select_random_with_probability(0.9)){
            sbit.grayscale_bits = config.significant_bits;
            sbit.red_bits = config.significant_bits;
            sbit.green_bits = config.significant_bits;
            sbit.blue_bits = config.significant_bits;
            sbit.alpha_bits = config.significant_bits;
        }
        else {
            sbit.grayscale_bits = rand() % UINT8_MAX;
            sbit.red_bits = rand() % UINT8_MAX;
            sbit.green_bits = rand() % UINT8_MAX;
            sbit.blue_bits = rand() % UINT8_MAX;
            sbit.alpha_bits = rand() % UINT8_MAX;
        }
    }
    
    
    uint8_t srgb_rendering_intent = rand() % UINT8_MAX;

    // Initializing spng_text text
    int n_text = rand() % 5;
    struct spng_text text[n_text];
    if (choose_to_initialize()){
        for (int i = 0; i < n_text; i++) {
            int length = rand() % 80;
            for (int j = 0; j < length; j++) {
                text[i].keyword[j] = rand() % 256;
            }
            text[i].keyword[length] = '\0';
            text[i].text = get_random_string(rand() % RECOMMENDED_MAX_LENGTH); // random string (at most 1000 chars)
            text[i].length = rand() % RECOMMENDED_MAX_LENGTH; // random length that can be larger than the actual string
            text[i].type = rand() % 5; // 1 to 3 are valid types, 0 and 4 are invalid
            text[i].compression_flag = rand() % UINT8_MAX;
            text[i].language_tag = get_random_string(rand() % RECOMMENDED_MAX_LENGTH);
            text[i].translated_keyword = get_random_string(rand() % RECOMMENDED_MAX_LENGTH);
        }
    }

    struct spng_bkgd bkgd;
    if (choose_to_initialize()){
        if (config.background == GRAY){
            bkgd.gray = rand() % UINT16_MAX;
        }
        else if (config.background == BLACK){
            bkgd.red = 0;
            bkgd.green = 0;
            bkgd.blue = 0;
        }
        else if (config.background == WHITE){
            bkgd.red = 255;
            bkgd.green = 255;
            bkgd.blue = 255;
        }
        else if (config.background == YELLOW){
            bkgd.red = 255;
            bkgd.green = 255;
            bkgd.blue = 0;
        } 
        else if (random_choice()){
            bkgd.red = rand() % UINT16_MAX;
            bkgd.green = rand() % UINT16_MAX;
            bkgd.blue = rand() % UINT16_MAX;
        }
        else if (random_choice()){
            bkgd.gray = rand() % UINT16_MAX;
        }
        else{
            bkgd.plte_index = rand() % UINT16_MAX;
        }
    }

    struct spng_hist hist;
    if (choose_to_initialize()){
        if (config.testFeature == HISTOGRAM && config.histogram_colors == 15) {
            for (int i = 0; i < 15; i++) {
                hist.frequency[i] = rand() % UINT16_MAX;
            }
            for (int i = 15; i < 256; i++) {
                hist.frequency[i] = 0;
            }
        } else {
            for (int i = 0; i < 256; i++) {
                hist.frequency[i] = rand() % UINT16_MAX;
            }
        }
    }

    struct spng_phys phys;
    if(choose_to_initialize()){
        if(config.testFeature == PHYSICAL_PIXELS){
            phys.ppu_x = config.ppu_x;
            phys.ppu_y = config.ppu_y;
            phys.unit_specifier = config.unit_specifier;
        }
        else{
            phys.ppu_x = rand() % UINT32_MAX;
            phys.ppu_y = rand() % UINT32_MAX;
            phys.unit_specifier = rand() % 3; // 0, 1, 2 (invalid)
        }
    }

    uint32_t n_splt = rand() % 5;
    struct spng_splt splt[n_splt];
    if(choose_to_initialize()){
        for (uint32_t i = 0; i < n_splt; i++) {
            int length = rand() % 80;
            for (int j = 0; j < length; j++) {
                splt[i].name[j] = rand() % 256;
            }
            splt[i].name[length] = '\0';
            splt[i].sample_depth = rand() % 3 * 8; // 0, 8, 16 which 0 is invalid
            splt[i].n_entries = rand() % RECOMMENDED_MAX_LENGTH;
            splt[i].entries = (struct spng_splt_entry*)malloc(splt[i].n_entries * sizeof(struct spng_splt_entry));
            for (uint32_t j = 0; j < splt[i].n_entries; j++) {
                splt[i].entries[j].red = rand() % UINT16_MAX;
                splt[i].entries[j].green = rand() % UINT16_MAX;
                splt[i].entries[j].blue = rand() % UINT16_MAX;
                splt[i].entries[j].alpha = rand() % UINT16_MAX;
                splt[i].entries[j].frequency = rand() % UINT16_MAX;
            }
        }
    }

    struct spng_time time;
    if(choose_to_initialize()){
        if(config.testFeature == TIME){
            time.year = config.time.year;
            time.month = config.time.month;
            time.day = config.time.day;
        }
        else{
            time.year = rand() % UINT16_MAX;
            time.month = rand() % UINT8_MAX;
            time.day = rand() % UINT8_MAX;
        }
        time.hour = rand() % UINT8_MAX;
        time.minute = rand() % UINT8_MAX;
        time.second = rand() % UINT8_MAX;
    }

    uint32_t n_chunks = rand() % 5;
    struct spng_unknown_chunk chunks[n_chunks];
    if(choose_to_initialize()){
        for (uint32_t i = 0; i < n_chunks; i++) {
            int length = rand() % 4;
            for (int j = 0; j < length; j++) {
                chunks[i].type[j] = rand() % 256;
            }
            chunks[i].type[length] = '\0';
            chunks[i].length = rand() % RECOMMENDED_MAX_LENGTH;
            chunks[i].data = get_random_string(rand() % RECOMMENDED_MAX_LENGTH);
            // location can be 0 (invalid), 1, 2 or 8
            int value = rand() % 4;
            if (value == 3) value = 8;
            chunks[i].location = value;

        }
    }

    struct spng_offs offs;
    if(choose_to_initialize()){
        offs.unit_specifier = rand() % 3; // 0, 1, 2 (invalid)
        offs.x = rand() % UINT32_MAX;
        offs.y = rand() % UINT32_MAX;
    }

    struct spng_exif exif;
    if(choose_to_initialize()){
        exif.data = get_random_string(rand() % RECOMMENDED_MAX_LENGTH);
        exif.length = rand() % RECOMMENDED_MAX_LENGTH;
    }

    struct buf_state state;
    state.data = NULL;
    state.bytes_left = rand() % SIZE_MAX;

    enum spng_option options_list[] = {
        SPNG_KEEP_UNKNOWN_CHUNKS, // true, false
        SPNG_IMG_COMPRESSION_LEVEL, // defaul -1, 0-9
        SPNG_IMG_WINDOW_BITS, // default 15, 8-15
        SPNG_IMG_MEM_LEVEL, // default 8, 1-9
        SPNG_IMG_COMPRESSION_STRATEGY, // default 0, 0-4
        SPNG_TEXT_COMPRESSION_LEVEL, // default -1, 0-9
        SPNG_TEXT_WINDOW_BITS, // default 15, 8-15
        SPNG_TEXT_MEM_LEVEL, // default 8, 1-9
        SPNG_TEXT_COMPRESSION_STRATEGY, // default 0, 0-4
        SPNG_FILTER_CHOICE, // default 0, 0-4
        SPNG_CHUNK_COUNT_LIMIT, // default 0, 0-UINT32_MAX
        SPNG_ENCODE_TO_BUFFER // true, false
    };
    int TOTAL_OPTIONS = sizeof(options_list) / sizeof(enum spng_option);

    // Set up random configuration
    int stream = rand() % 2;
    int get_buffer = rand() % 2;
    int progressive = rand() % 2;
    int fmt = get_fmt_from_config(&config);

    int num_options = rand() % TOTAL_OPTIONS;
    int chosen_options[num_options+1];
    int chosen_values[num_options+1];
    choose_random_options(options_list, TOTAL_OPTIONS, num_options, chosen_options, chosen_values);

    // Print configuration
    printf("Configuration:\n");
    printf(" - stream: %d\n", stream);
    printf(" - get_buffer: %d\n", get_buffer);
    printf(" - progressive: %d\n", progressive);
    printf(" - fmt: %d\n", fmt);
    printf(" - num_options: %d\n", num_options);

    for(int i = 0; i < num_options; i++){
        printf(" - Option %d, Value %d\n", chosen_options[i], chosen_values[i]);
    }
    
    // end Initialization

    // print version
    printf("libspng version: %s\n", spng_version_string());

    // Test spng_ctx_new
    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if(ctx == NULL) goto err;

    test(spng_set_image_limits(ctx, 200000, 200000));

    int limits = 4 * 1000 * 1000;
    test(spng_set_chunk_limits(ctx, limits, limits * 2));
    
    spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, 1);

    if(stream){
        test(spng_set_png_stream(ctx, stream_write_fn, &state));
    }
    else{
        test(spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 1));
    } 

    if(fn_ret) goto err;

    for(int i = 0; i < num_options; i++){
        test(spng_set_option(ctx, chosen_options[i], chosen_values[i]));
    }

    test(spng_set_ihdr(ctx, &ihdr));
    if(fn_ret) goto err;

    test(spng_set_plte(ctx, &plte));
    test(spng_set_trns(ctx, &trns));
    test(spng_set_chrm(ctx, &chrm));
    test(spng_set_chrm_int(ctx, &chrm_int));
    test(spng_set_gama(ctx, gama));
    test(spng_set_gama_int(ctx, gama_int));
    test(spng_set_iccp(ctx, &iccp));
    test(spng_set_sbit(ctx, &sbit));
    test(spng_set_srgb(ctx, srgb_rendering_intent));
    test(spng_set_text(ctx, text, n_text));
    test(spng_set_bkgd(ctx, &bkgd));
    test(spng_set_hist(ctx, &hist));
    test(spng_set_phys(ctx, &phys));
    test(spng_set_splt(ctx, splt, n_splt));
    test(spng_set_time(ctx, &time));
    test(spng_set_unknown_chunks(ctx, chunks, n_chunks));
    test(spng_set_offs(ctx, &offs));
    
    // ERROR FOUND
    // Compilation with -Onumber causes segmentation fault if there is no exif data
    // for some configuration it throw segmentation fault even if there is exif data
    test(spng_set_exif(ctx, &exif));
    

    // Test colorspace
    switch(ihdr.color_type)
    {
        case SPNG_COLOR_TYPE_GRAYSCALE:
            pixel_bits = ihdr.bit_depth;
            break;
        case SPNG_COLOR_TYPE_INDEXED:
            pixel_bits = ihdr.bit_depth;
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR:
            pixel_bits = ihdr.bit_depth * 3;
            break;
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
            pixel_bits = ihdr.bit_depth * 2;
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
            pixel_bits = ihdr.bit_depth * 4;
            break;
        default: goto err;
    }
    img_size = ihdr.width * pixel_bits + 7;
    img_size /= 8;
    img_size *= ihdr.height;

    if(img_size > size) goto err;
    if(img_size > 80000000) goto err;

    img = (unsigned char*)data;

    if(progressive)
    {
        // test scanline
        test(spng_encode_scanline(ctx, img, img_size));

        test(spng_encode_image(ctx, NULL, 0, fmt, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE));
        if(fn_ret) goto err;

        // test encode_chunks
        test(spng_encode_chunks(ctx));

        // test row
        size_t ioffset, img_width = img_size / ihdr.height;
        struct spng_row_info ri = {0};

        do
        {
            if(spng_get_row_info(ctx, &ri)) break;
            ioffset = ri.row_num * img_width;
        }while(!spng_encode_row(ctx, img + ioffset, img_size));
    }
    else{
        test(spng_encode_image(ctx, img, img_size, fmt, SPNG_ENCODE_FINALIZE));
        if(fn_ret) goto err;
    }

    if(get_buffer)
    {
        png = spng_get_png_buffer(ctx, &png_size, &fn_ret);

        // These are potential vulnerabilities
        if((png && !png_size) || (!png && png_size) || (fn_ret && (png || png_size))){

            // Test spng_ctx_free
            if(ctx != NULL){
                printf("Testing spng_ctx_free...");
                spng_ctx_free(ctx);
                printf("OK\n");
            }
            
            // free splt entries    
            for(uint32_t i = 0; i < n_splt; i++)
            {
                if(splt[i].entries != NULL) free(splt[i].entries);
            }

            // free iccp profile
            if(iccp.profile != NULL) free(iccp.profile);

            // free text
            for(int i = 0; i < n_text; i++)
            {
                if(text[i].text != NULL) free(text[i].text);
                if(text[i].language_tag != NULL) free(text[i].language_tag);
                if(text[i].translated_keyword != NULL) free(text[i].translated_keyword);
            }

            // free exif data
            if(exif.data != NULL) free(exif.data);

            // free unknown chunks
            for(uint32_t i = 0; i < n_chunks; i++)
            {
                if(chunks[i].data != NULL) free(chunks[i].data);
            }

            // end spng_ctx_free
            if(png != NULL) free(png);
            return 1;
        }
    }

    // Test spng_ctx_free    
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 
    // end spng_ctx_free

    // free splt entries    
    for(uint32_t i = 0; i < n_splt; i++)
    {
        if(splt[i].entries != NULL) free(splt[i].entries);
    }

    // free iccp profile
    if(iccp.profile != NULL) free(iccp.profile);

    // free text
    for(int i = 0; i < n_text; i++)
    {
        if(text[i].text != NULL) free(text[i].text);
        if(text[i].language_tag != NULL) free(text[i].language_tag);
        if(text[i].translated_keyword != NULL) free(text[i].translated_keyword);
    }

    // free exif data
    if(exif.data != NULL) free(exif.data);

    // free unknown chunks
    for(uint32_t i = 0; i < n_chunks; i++)
    {
        if(chunks[i].data != NULL) free(chunks[i].data);
    }

    printf("Finished\n");
    if(png != NULL) free(png);
    return 0;

err:
    // Test spng_ctx_free    
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 

    // free splt entries    
    for(uint32_t i = 0; i < n_splt; i++)
    {
        if(splt[i].entries != NULL) free(splt[i].entries);
    }

    // free iccp profile
    if(iccp.profile != NULL) free(iccp.profile);

    // free text
    for(int i = 0; i < n_text; i++)
    {
        if(text[i].text != NULL) free(text[i].text);
        if(text[i].language_tag != NULL) free(text[i].language_tag);
        if(text[i].translated_keyword != NULL) free(text[i].translated_keyword);
    }

    // free exif data
    if(exif.data != NULL) free(exif.data);

    // free unknown chunks
    for(uint32_t i = 0; i < n_chunks; i++)
    {
        if(chunks[i].data != NULL) free(chunks[i].data);
    }

    // end spng_ctx_free
    printf("Finished with error\n");
    if(png != NULL) free(png);
    return 0;
}