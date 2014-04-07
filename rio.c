#include "rio.h"

ssize_t readlineb(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t r;

    if ((r = rio_readlineb(rp, usrbuf, n)) < 0)
    {
        fprintf(stderr, "rio_readline error\n");
        close(rp->rio_fd);
    }

    return r;
}

void writen(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
    {
        fprintf(stderr, "rio_writen error\n");
        close(fd);
    }
}
