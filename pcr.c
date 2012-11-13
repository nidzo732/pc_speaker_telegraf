#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
	FILE *f=fopen("/dev/telegraf", "r+");
	char *c=malloc(strlen("Modul radi za citanje")+1);
	fread(c, 1, strlen("Modul radi za citanje"), f);
	printf("\n%s\n", c);
}
