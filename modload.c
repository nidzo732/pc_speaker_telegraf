/*Program za ucitavanje modula i kreiranje fajla uredjaja*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEVNAME "telegraf"
#define MODNAME "pc_speaker_telegraf.ko"

int main(int argc, char **argv)
{
	char *komanda=malloc(strlen("insmod")+strlen(MODNAME)+1);
	strcat(komanda, "insmod ");
	strcat(komanda, MODNAME);
	system(komanda);
	free(komanda);
	char *fdevnum=malloc(50);
	char *fdevname=malloc(50);
	FILE *proc_devices=fopen("/proc/devices", "r");
	fscanf (proc_devices, "%s %s", fdevnum, fdevname);
	while (fscanf(proc_devices, "%s %s", fdevnum, fdevname)==2)
	{
		if (strcmp(fdevname, DEVNAME)==0)
		{
			char *komanda=malloc(strlen(fdevname)+strlen(fdevnum)+strlen("mknod /dev/ c 0")+10);
			strcpy(komanda, "mknod /dev/");
			strcat(komanda, DEVNAME);
			strcat(komanda, " c ");
			strcat(komanda, fdevnum);
			strcat(komanda, " 0");
			system(komanda);
			return 0;
		}
	}
	return 1;
}

