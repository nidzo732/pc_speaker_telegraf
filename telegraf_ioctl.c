/*Program za demonstraciju ioctl funkcije*/
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>


int main(void)
{
	int f=open("/dev/telegraf", O_RDWR);
	unsigned int command;
	unsigned long param;
	scanf("%u", &command);
	scanf("%lu", &param);
	printf("%d\n", ioctl(f, command, param));
}
