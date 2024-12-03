#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

int fuzz_spng_read(const uint8_t* data, size_t size);
int fuzz_spng_write(const uint8_t* data, size_t size);

#define test(fn) fn_ret = fn; if(fn_ret){fprintf(stderr, "Error: %s returned %d: %s\n", #fn, fn_ret, spng_strerror(fn_ret)); goto error;}

int main(int argc, char **argv)
{
    srand(time(NULL));

    FILE *f = NULL;
    char *buf = NULL;
    long siz_buf;

    if(argc < 2)
    {
        fprintf(stderr, "no input file\n");
        goto error;
    }

    f = fopen(argv[1], "rb");
    if(f == NULL)
    {
        fprintf(stderr, "error opening input file %s\n", argv[1]);
        goto error;
    }

    // get file size
    fseek(f, 0, SEEK_END);
    siz_buf = ftell(f);
    rewind(f);
    if(siz_buf < 1) goto error;

    // allocate buffer
    buf = (char*)malloc(siz_buf);
    if(buf == NULL)
    {
        fprintf(stderr, "malloc() failed\n");
        goto error;
    }

    // read file
    if(fread(buf, siz_buf, 1, f) != 1)
    {
        fprintf(stderr, "fread() failed\n");
        goto error;
    }

    int success = 0;
    success = fuzz_spng_read((const uint8_t*)buf, siz_buf);
    //success = fuzz_spng_write((const uint8_t*)buf, siz_buf);

    free(buf);
    if (f != NULL) fclose(f);

    return success;

error:
    free(buf);
    if (f != NULL) fclose(f);

    return 1;
}
