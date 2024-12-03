#include <spng.h>
#include <string.h>
#include <stdio.h>

// FUNCTIONS TAKEN FROM spng.c
struct buf_state
{
    const uint8_t *data;
    size_t bytes_left;
};

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
// end spng.c

#define test(fn) \
    printf("Testing %s... ", #fn); \
    fn_ret = fn; \
    if(fn_ret){ \
        printf("\nError: %s returned %d: %s\n", #fn, fn_ret, spng_strerror(fn_ret)); \
        goto err; \
    } \
    printf("OK\n");

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
    int stream = rand() % 2;
    int file_stream = rand() % 2;
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