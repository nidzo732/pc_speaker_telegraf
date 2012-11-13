#Be advised, this makefile relies on the fact that /lib/modules/version/build
#is linked to the sources or headers of the current kernel.
#If that isn't the case, adjust those parts to point to the location of your
#kernel headers or sources
obj-m := pc_speaker_telegraf.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
