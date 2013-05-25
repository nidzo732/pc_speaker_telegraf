/*Program za uklanjanje modula i brisanje fajla*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEVNAME "telegraf"
#define MODNAME "pc_speaker_telegraf"

int main(int argc, char **argv)
{
	char *komanda=malloc(strlen("rmmod ")+strlen(MODNAME)+1);
	strcat(komanda, "rmmod ");
	strcat(komanda, MODNAME);
	system(komanda);
	komanda[0]='\0';
	strcat(komanda, "rm /dev/");
	strcat(komanda, DEVNAME);
	system(komanda);
	return 0;
}


