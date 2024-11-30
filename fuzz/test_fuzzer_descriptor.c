#include <fcntl.h>
#include <unistd.h>

#define NUM_CHARS 1024

int main()
{
    int fd = open("/dev/zero", O_RDONLY);
    if (fd < 0)
    {
        write(STDOUT_FILENO, "Open failed\n", 12);
        return 1;
    }
    char buf[NUM_CHARS + 1] = {0};

    read(fd, buf, NUM_CHARS);

    write(STDOUT_FILENO, buf, NUM_CHARS);

    close(fd);
    return 0;
}