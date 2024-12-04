#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <spng.h>
#include <unistd.h>
#include <fcntl.h>

#define FUZZ_READ 1

#define test(fn)                                                          \
    printf("Testing %s... ", #fn);                                                   \
    fn_ret = fn;                                                                     \
    if (fn_ret)                                                                      \
    {                                                                                \
        printf("\nError: %s returned %d: %s\n", #fn, fn_ret, spng_strerror(fn_ret)); \
        goto err;                                                                    \
    }                                                                                \
    printf("OK\n");

// DECLARATION:
int fuzz_spng_read(const uint8_t* data, size_t size);
int fuzz_spng_write(const uint8_t* data, size_t size);

struct buf_state;
static int buffer_read_fn(spng_ctx *ctx, void *user, void *dest, size_t length);

////////////////////////////////////////
// MAIN:
////////////////////////////////////////

int main(int argc, char **argv)
{
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

    if(siz_buf < 1) goto error;

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

    int success = 0;

#if FUZZ_READ == 1
    success = fuzz_spng_read((const uint8_t *)buf, siz_buf);
#else
    success = fuzz_spng_write((const uint8_t *)buf, siz_buf);
#endif

    free(buf);
    if (fd != -1) 
        close(fd);

    return success;

error:
    free(buf);
    if (fd != -1) 
        close(fd);

    return 1;
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
        switch (chosen_options[i])
        {
        case SPNG_KEEP_UNKNOWN_CHUNKS:
            chosen_values[i] = rand() % 2;
            break;
        case SPNG_IMG_COMPRESSION_LEVEL:
            chosen_values[i] = rand() % 11 - 1;
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
            chosen_values[i] = rand() % 11 - 1;
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
            chosen_values[i] = rand() % 5;
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
        SPNG_ENCODE_TO_BUFFER // true, false
    };
    int TOTAL_OPTIONS = sizeof(options_list) / sizeof(enum spng_option);

    // Set up random configuration
    int stream = 1;
    int file_stream = 1;
    int discard = rand() % 2;
    int progressive = rand() % 2;
    int fmt = fmt_flags[rand() % TOTAL_TMP_FLAGS];
    fmt = SPNG_FMT_RGBA8;
    int flags = decode_flags[rand() % TOTAL_DECODE_FLAGS];
    
    /*
    int num_options = rand() % TOTAL_OPTIONS;
    int chosen_options[num_options];
    int chosen_values[num_options];
    choose_random_options(options_list, TOTAL_OPTIONS, num_options, chosen_options, chosen_values);
    */

    // Print configuration
    printf("stream: %d\n", stream);
    printf("file_stream: %d\n", file_stream);
    printf("discard: %d\n", discard);
    printf("progressive: %d\n", progressive);
    printf("fmt: %d\n", fmt);
    printf("flags: %d\n", flags);
    //printf("num_options: %d\n", num_options);
    
    // end Initialization

    // Test spng_ctx_new
    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_IGNORE_ADLER32);
    test(ctx == NULL);

    if(stream)
    {
        if(file_stream) {
            // Simulate a file stream from data
            file = fmemopen((void*)data, size, "rb");

            test(file == NULL);
            
            test(spng_set_png_file(ctx, file));
        }
        else {
            test(spng_set_png_stream(ctx, buffer_read_fn, &state));
        }
    }
    else{
        test(spng_set_png_buffer(ctx, (void*)data, size));
    }

    test(spng_set_image_limits(ctx, 200000, 200000));
        
    int limits = 4 * 1000 * 1000;
    test(spng_set_chunk_limits(ctx, limits, limits * 2));

    test(spng_set_crc_action(ctx, SPNG_CRC_USE, discard ? SPNG_CRC_DISCARD : SPNG_CRC_USE))

    test(spng_set_option(ctx, SPNG_KEEP_UNKNOWN_CHUNKS, 1));

    /*
    for(int i = 0; i < num_options; i++){
        printf("Option %d: %d -> %d\n", i, chosen_options[i], chosen_values[i]);
        test(spng_set_option(ctx, chosen_options[i], chosen_values[i]));
    }
    */

    size_t out_size;
    test(spng_decoded_image_size(ctx, fmt, &out_size));

    test(out_size > 800000000);

    img = (unsigned char*)malloc(out_size);
    
    test(img == NULL);

    //// Test get methods
    test(spng_get_ihdr(ctx, &ihdr));
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
    test(spng_get_text(ctx, text, &n_text));
    test(spng_get_text(ctx, NULL, &n_text));
    for(uint32_t i = 0; i < n_text; i++)
    {
        // All strings should be non-NULL
        test(text[i].text == NULL);
        test(text[i].keyword[0] == '\0');
        test(text[i].language_tag == NULL);
        test(text[i].translated_keyword == NULL);
        test(memchr(text[i].keyword, 0, 80) == NULL);
        
        /* This shouldn't cause issues either */
        text[i].length = strlen(text[i].text);
    }

    test(spng_get_bkgd(ctx, &bkgd));
    test(spng_get_hist(ctx, &hist));
    test(spng_get_phys(ctx, &phys));

    // Test spng_get_splt for 4 and for arbitrary number
    test(spng_get_splt(ctx, splt, &n_splt));
    test(spng_get_splt(ctx, NULL, &n_splt));
    for(uint32_t i = 0; i < n_splt; i++)
    {
        test(splt[i].name[0] == '\0');
        test(splt[i].entries == NULL);
        test(memchr(splt[i].name, 0, 80) == NULL);
    }

    // Test spng_get_unknown_chunks for 4 and for arbitrary number
    test(spng_get_unknown_chunks(ctx, chunks, &n_chunks));
    test(spng_get_unknown_chunks(ctx, NULL, &n_chunks));
    for(uint32_t i = 0; i < n_chunks; i++)
    {
        test(chunks[i].length && !chunks[i].data);
        test(!chunks[i].length && chunks[i].data);
    }
    
    if(progressive)
    {
        test(spng_decode_image(ctx, NULL, 0, fmt, flags | SPNG_DECODE_PROGRESSIVE));

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

    return 1;
}

/// @brief Fuzz function for spng_write
/// @param data - data to write
/// @param size - size of data
/// @return - 0 on success, 1 on failure
int fuzz_spng_write(const uint8_t* data, size_t size)
{
    // Initialization
    int fn_ret;
    unsigned char *img = NULL;
    size_t img_size;
    size_t pixel_bits = 0;

    void *png = NULL;
    size_t png_size = 0;

    struct spng_ihdr ihdr;
    struct spng_plte plte;
    struct spng_trns trns;
    struct spng_chrm chrm;
    struct spng_chrm_int chrm_int;
    double gama = 0.45455;
    uint32_t gama_int = 45455;
    struct spng_iccp iccp;
    struct spng_sbit sbit;
    uint8_t srgb_rendering_intent = 1;
    struct spng_text text[4] = {0};
    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_phys phys;
    struct spng_splt splt[4] = {0};
    struct spng_time time;
    struct spng_unknown_chunk chunks[4] = {0};
    uint32_t n_text = 4, n_splt = 4, n_chunks = 4;

    struct buf_state state;
    state.data = NULL;
    state.bytes_left = SIZE_MAX;

    // Initialize enums from spng.h
    enum spng_format fmt_flags[] = {
        SPNG_FMT_RGBA8, SPNG_FMT_RGBA16, SPNG_FMT_RGB8,
        SPNG_FMT_GA8, SPNG_FMT_GA16, SPNG_FMT_G8,
        SPNG_FMT_PNG, SPNG_FMT_RAW
    };
    int TOTAL_TMP_FLAGS = sizeof(fmt_flags) / sizeof(enum spng_format);

    // Set up random configuration
    int stream = rand() % 2;
    int get_buffer = rand() % 2;
    int progressive = rand() % 2;
    int fmt = fmt_flags[rand() % TOTAL_TMP_FLAGS];

    // end Initialization

    // Test spng_ctx_new
    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    test(ctx == NULL);

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

    test(spng_set_ihdr(ctx, &ihdr));
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

    test(img_size > size);
    test(img_size > 80000000);

    img = (unsigned char*)data;

    if(progressive)
    {
        test(spng_encode_image(ctx, NULL, 0, fmt, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE));

        size_t ioffset, img_width = img_size / ihdr.height;
        struct spng_row_info ri;

        do
        {
            if(spng_get_row_info(ctx, &ri)) break;
            ioffset = ri.row_num * img_width;
        }while(!spng_encode_row(ctx, img + ioffset, img_size));
    }
    else{
        test(spng_encode_image(ctx, img, img_size, fmt, SPNG_ENCODE_FINALIZE));
    }

    if(get_buffer)
    {
        png = spng_get_png_buffer(ctx, &png_size, &fn_ret);

        // These are potential vulnerabilities
        test(png && !png_size);
        test(!png && png_size);
        test(fn_ret && (png || png_size));
    }

    // Test spng_ctx_free    
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 
    // end spng_ctx_free

    if(png != NULL) free(png);
    return 0;

err:
    // Test spng_ctx_free    
    if(ctx != NULL){
        printf("Testing spng_ctx_free...");
        spng_ctx_free(ctx);
        printf("OK\n");
    } 
    // end spng_ctx_free

    if(png != NULL) free(png);
    return 1;
}