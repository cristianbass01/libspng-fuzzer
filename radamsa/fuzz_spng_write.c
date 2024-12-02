#include <spng.h>
#include <string.h>

struct buf_state
{
    const uint8_t *data;
    size_t bytes_left;
};

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


#define test(fn) fn_ret=fn; if(fn_ret){fprintf(stderr, "Error: %s returned %d: %s\n", #fn, fn_ret, spng_strerror(fn_ret)); goto error;}

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
        default: goto error;
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
    spng_ctx_free(ctx);
    // end spng_ctx_free

    free(png);
    return 0;

error:
    // Test spng_ctx_free    
    spng_ctx_free(ctx);
    // end spng_ctx_free

    free(png);
    return 1;
}
