#include <stdio.h>
#include <stdint.h>
#include <unistd.h>	/* read(), write(), close() */
#include <fcntl.h>	
uint8_t buf[4]={0};
int main()
{
    int fd = open("/dev/aic122020",O_WRONLY);
    if (fd==-1)
    {
        printf("error opening device\n");
        exit(0);
    }

    buf[0] = 0;
    buf[1] = 1;
    buf[2] = 0xfe;
    buf[3] = 0;
    write(fd,buf,4);
    close(fd);
}
