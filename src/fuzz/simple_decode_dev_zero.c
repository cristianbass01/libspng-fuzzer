#include <spng.h>
#include <stdio.h>

int main()
{
    spng_ctx *ctx = spng_ctx_new(0);
    size_t img_size;
    FILE *f = fopen("/dev/zero", "rb");
    if (f == NULL)
    {
        printf("File not found\n");
        return 1;
    }

    if (ctx == NULL)
    {
        return 1;
    }

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    spng_set_png_file(ctx, f);

    int r = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &img_size);
    if (r)
    {
        return 1;
    }

    void *image = malloc(img_size);
    if (image == NULL)
    {
        return 1;
    }

    r = spng_decode_image(ctx, image, img_size, SPNG_FMT_RGBA8, 0);
    if (r)
    {
        return 1;
    }

    spng_ctx_free(ctx);
    free(image);
    fclose(f);

    return 0;
}