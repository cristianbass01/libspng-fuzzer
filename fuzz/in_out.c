#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        write(STDOUT_FILENO, "Usage: ./in_out <input_file> <output_file>\n", 43);
        return 1;
    }
    int fd_read = open(argv[1], O_RDONLY);
    if (fd_read < 0)
    {
        write(STDOUT_FILENO, "Open failed\n", 12);
        return 1;
    }
    FILE *write_file = fopen(argv[2], "w");
    if (write_file == NULL)
    {
        write(STDOUT_FILENO, "Open failed\n", 12);
        return 1;
    }

    long len = lseek(fd_read, 0, SEEK_END);
    char *buf = (char *)malloc(len);
    lseek(fd_read, 0, SEEK_SET);

    read(fd_read, buf, len);

    fwrite(buf, 1, len, write_file);

    free(buf);
    close(fd_read);
    fclose(write_file);
    return 0;
}