#include <linux/init.h> 	/*module_init(), module_exit(), __init, __exit*/
#include <linux/module.h>	/*za sve module*/
#include <linux/kdev_t.h>	/*dev_t*/
#include <linux/errno.h>	/*makroi za greske*/
#include <linux/moduleparam.h>	/*module_param()*/
#include <linux/sched.h>	/*current*/
#include <linux/fs.h>		/*fops, alloc_chrdev_region(), unregister_chrdev_region(),*/
#include <linux/cdev.h>		/*cdev, cdev_init, cdev_alloc, cdev_add, cdev_del*/
#include <linux/slab.h>		/*kmalloc(), GFP_KERNEL	*/
#include <linux/string.h>	/*strcpy(), strcat(), strlen()...*/
#include <asm/uaccess.h>	/*copy_to_user(), copy_from_user()*/
#include <linux/semaphore.h>	/*struct semaphore, sema_init(), dow_interruptible(), up() */
#include <linux/ioctl.h>	/*request_region()*/
#include <linux/delay.h> 	/*msleep_interruptible()*/
#include <linux/kthread.h>	/*task struct, kthread_create(), wake_up_process()*/
#include <linux/completion.h>	/*struct completion, init_completion(), complete()*/
#include <linux/capability.h>	/*capable()	*/
#include <linux/ioport.h>	/*inb(), outb()*/
#include <asm/io.h>		/*-||-*/
#include <linux/spinlock.h>  	/*spin_lock_irqsave(), spin_lock_irqrestore(), spinlock_t*/
#include <linux/version.h>
#include <linux/i8253.h>
#if LINUX_VERSION_CODE<=KERNEL_VERSION(3, 3, 0)
	#include <asm/system.h>			/*smp_mb()*/
#else
	#include <asm/barrier.h>
#endif

MODULE_DESCRIPTION("A module that translates text written into /dev/telegraf and\
plays it over the PC speaker");
MODULE_AUTHOR("Nikola Pavlović <nikola825@gmail.com>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

#define DEFAULTFREQ 500	
#define DEVNAME "telegraf"
#define DEVCOUNT 1			
#define READ_RESPONSE "This file isn't ment to be read\ntype modinfo pc_speaker_telegraf for more information\n"
#define PLAY_THREAD_NAME "playthread"
#define TIMERFREQ 1193180			
#define DEFAULT_DOTLENGTH 100		
#define DOTSLEEP msleep_interruptible(dotlength)	
#define LETTERSLEEP msleep_interruptible(dotlength*2)
#define CONTROLL_CODE_UPDATE_COUNTDOWN 0xb6
#define PIT2_CONTROLL_PORT 0x43
#define COUNTDOWN_UPDATE_PORT 0x42
#define ON_OFF_PORT 0x61
#define IOCTLCODE_SETFREQ 1
#define IOCTLCODE_SETDOTLENGTH 3

static int  __init pc_speaker_telegraf_init(void);
static void __exit pc_speaker_telegraf_exit(void);	
static ssize_t telegraf_read(struct file *read_file, char __user *readbuffer, size_t read_size, loff_t *location); 
static ssize_t telegraf_write(struct file *write_file, const char __user *writebuffer, size_t write_size, loff_t *location); 
static long telegraf_ioctl(struct file *telegraf_file, unsigned int ioctl_command, unsigned long ioctl_args); 
static void playchar(char letter);
static int playstring(void *str);
static void playdot(void);
static void playdash(void);

static struct file_operations telegraf_fops=
{
	.owner=				THIS_MODULE,
	.read=				telegraf_read,
	.write=				telegraf_write,
	.unlocked_ioctl=	telegraf_ioctl,
};

//typedef bool char;
static enum plstat{FORCE_STOP, GO_ON} play_status=GO_ON;		
static struct cdev telegraf_cdev;		
static dev_t telegraf_devnum;			
static int freq=DEFAULTFREQ;			
static int countdown=TIMERFREQ/DEFAULTFREQ;	
static int dotlength=DEFAULT_DOTLENGTH;		
static struct semaphore *write_semafor=NULL, *ioctl_semafor=NULL, *read_semafor=NULL;	
static struct completion *write_completion=NULL;
static unsigned int highbyte=(TIMERFREQ/DEFAULTFREQ)/256;
static unsigned int lowbyte=(TIMERFREQ/DEFAULTFREQ)-((TIMERFREQ/DEFAULTFREQ)/256);
static bool dev_ok=false, cdev_ok=false;

module_param(freq, int, S_IRUGO);
module_param(dotlength, int, S_IRUGO);

module_init(pc_speaker_telegraf_init);
module_exit(pc_speaker_telegraf_exit);

static int  __init pc_speaker_telegraf_init(void)
{
	int errno;
	//if (request_region(ON_OFF_PORT, 0x01, "controll_register")==NULL) return -ENODEV;
	//if (request_region(COUNTDOWN_UPDATE_PORT, 0x02, "tajmer")==NULL) return -ENODEV;
	write_semafor=kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	ioctl_semafor=kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	read_semafor=kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	write_completion=kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (write_semafor==NULL || read_semafor==NULL || ioctl_semafor==NULL || write_completion==NULL)
		goto structure_alloc_fail;
	sema_init(write_semafor, 1);
	sema_init(ioctl_semafor, 1);
	sema_init(read_semafor, 1);
	init_completion(write_completion);
	complete(write_completion);
	errno=alloc_chrdev_region(&telegraf_devnum, 0, DEVCOUNT, DEVNAME);
	if (errno!=0) goto chrdev_alloc_fail; else dev_ok=true;
	cdev_init(&telegraf_cdev, &telegraf_fops);
	errno=cdev_add(&telegraf_cdev, telegraf_devnum, 1);
	if (errno!=0) goto cdev_alloc_fail; else cdev_ok=true;
	return 0;
	
	cdev_alloc_fail:
		cdev_del(&telegraf_cdev);
	chrdev_alloc_fail:
		complete(write_completion);
		unregister_chrdev_region(telegraf_devnum, DEVCOUNT);
		kfree(write_semafor);
		kfree(read_semafor);
		kfree(ioctl_semafor);
		kfree(write_completion);
		return -ENODEV;
	structure_alloc_fail:
		if (write_semafor!=NULL) kfree(write_semafor);
		if (read_semafor!=NULL) kfree(read_semafor);
		if (ioctl_semafor!=NULL) kfree(ioctl_semafor);
		if (write_completion!=NULL) kfree(write_completion);
		return -ENOMEM;
}
static void __exit pc_speaker_telegraf_exit(void)
{
	play_status=FORCE_STOP;
	wait_for_completion(write_completion);
	if (cdev_ok) cdev_del(&telegraf_cdev);
	if (write_semafor!=NULL) kfree(write_semafor);
	if (read_semafor!=NULL) kfree(read_semafor);
	if (ioctl_semafor!=NULL) kfree(ioctl_semafor);
	if (write_completion!=NULL) kfree(write_completion);
	if (dev_ok) unregister_chrdev_region(telegraf_devnum, DEVCOUNT);
	//release_region(ON_OFF_PORT, 0x01);
}

static ssize_t telegraf_read(struct file *read_file, char __user *readbuffer, size_t read_size, loff_t *location)
{
	char *response;
	static enum wr{YES, NO} wasread=NO;	
	int data_remaining;
	down_interruptible(read_semafor);
	if (wasread==YES)
	{
		wasread=NO;
		up(read_semafor);
		return 0;
	}
	response=kmalloc(sizeof(READ_RESPONSE)+1, GFP_KERNEL);
	if (response==NULL)
	{
		up(read_semafor);
		return -ENOMEM;
	}
	strcpy(response, READ_RESPONSE);
	if (read_size<=strlen(response))
	{
		wasread=YES;
		data_remaining=copy_to_user(readbuffer, response, read_size);
		kfree(response);
		up(read_semafor);
		if (data_remaining) return -EFAULT;
		else return read_size;
	}
	else
	{
		wasread=YES;
		data_remaining=copy_to_user(readbuffer, response, strlen(response));
		kfree(response);
		up(read_semafor);
		if (data_remaining) return -EFAULT;
		else return read_size;
	}
}
static ssize_t telegraf_write(struct file *write_file, const char __user *writebuffer, size_t write_size, loff_t *location)
{
	struct task_struct *playthread;
	char *towrite;
	int data_remaining;
	towrite=kmalloc(write_size+1, GFP_KERNEL);
	if (towrite==NULL)
	{
		printk(KERN_ERR "Failed to allocate memory for input from process %s (PID: %d),\
failed to allocate %lu bytes of memmory", current->comm, current->pid, write_size+1);
		return -ENOMEM;
	}
	data_remaining=copy_from_user(towrite, writebuffer, write_size);
	if (data_remaining)
	{
		kfree(towrite);
		return -EFAULT;
	}
	towrite[write_size-1]='\0';
	if (play_status==FORCE_STOP) 
	{
		kfree(towrite);
		return write_size;
	}
	playthread=kthread_create(playstring, (void*)towrite, PLAY_THREAD_NAME);
	wake_up_process(playthread);	
	return write_size;
}

static int playstring(void *strw)
{
	size_t i;
	size_t wlen;
	char *str=(char*)strw;
	if (play_status==FORCE_STOP) 
	{
		kfree(str);
		return 0;
	}
	down_interruptible(write_semafor);
	init_completion(write_completion);	
	wlen=strlen(str);
	for (i=0;i<wlen;i++)
	{
		if (play_status==FORCE_STOP) 
		{
			kfree(str);
			up(write_semafor);
			complete_and_exit(write_completion, 0);
		}
		playchar(str[i]);
	}
	kfree(str);
	up(write_semafor);
	complete_and_exit(write_completion, 0);
}

static long telegraf_ioctl(struct file *telegraf_file, unsigned int ioctl_command, unsigned long ioctl_args)
{
	if ( !capable(CAP_SYS_ADMIN)) return -EPERM;
	switch(ioctl_command)
	{
		case IOCTLCODE_SETFREQ:
		down_interruptible(ioctl_semafor);
		freq=ioctl_args;
		countdown=TIMERFREQ/freq;
		countdown%=65536;
		highbyte=countdown>>8;
		lowbyte=countdown-highbyte;
		up(ioctl_semafor);
		return 0;
		
		case IOCTLCODE_SETDOTLENGTH:
		dotlength=ioctl_args;
		return 0;
		
		default:
		return -ENOTTY;		
	}
	return 0;
}

static void playchar(char letter)
{
	if (letter>=65 && letter<=90) letter+=32;
	switch(letter)
	{
		case 'a':
			playdot();
			playdash();
			break;
		case 'b':
			playdash();
			playdot();
			playdot();
			playdot();
			break;
		case 'c':
			playdash();
			playdot();
			playdash();
			playdot();
			break;
		case 'd':
			playdash();
			playdot();
			playdot();
			break;
		case 'e':
			playdot();
			break;
		case 'f':
			playdot();
			playdot();
			playdash();
			playdot();
			break;
		case 'g':
			playdash();
			playdash();
			playdot();
			break;
		case 'h':
			playdot();
			playdot();
			playdot();
			playdot();
			break;
		case 'i':
			playdot();
			playdot();
			break;
		case 'j':
			playdot();
			playdash();
			playdash();
			playdash();
			break;
		case 'k':
			playdash();
			playdot();
			playdash();
			break;
		case 'l':
			playdot();
			playdash();
			playdot();
			playdot();;
			break;
		case 'm':
			playdash();
			playdash();
			break;
		case 'n':
			playdash();
			playdot();
			break;
		case 'o':
			playdash();
			playdash();
			playdash();
			break;
		case 'p':
			playdot();
			playdash();
			playdash();
			playdot();
			break;
		case 'q':
			playdash();
			playdash();
			playdot();
			playdash();
			break;
		case 'r':
			playdot();
			playdash();
			playdot();
			break;
		case 's':
			playdot();
			playdot();
			playdot();
			break;
		case 't':
			playdash();
			break;
		case 'u':
			playdot();
			playdot();
			playdash();
			break;
		case 'v':
			playdot();
			playdot();
			playdot();
			playdash();
			break;
		case 'w':
			playdot();
			playdash();
			playdash();
			break;
		case 'x':
			playdash();
			playdot();
			playdot();
			playdash();
			break;
		case 'y':
			playdash();
			playdot();
			playdash();
			playdash();
			break;
		case 'z':
			playdash();
			playdash();
			playdot();
			playdot();
			break;
		case '1':
			playdot();
			playdash();
			playdash();
			playdash();
			playdash();
			break;
		case '2':
			playdot();
			playdot();
			playdash();
			playdash();
			playdash();
			break;
		case '3':
			playdot();
			playdot();
			playdot();
			playdash();
			playdash();
			break;
		case '4':
			playdot();
			playdot();
			playdot();
			playdot();
			playdash();
			break;
		case '5':
			playdot();
			playdot();
			playdot();
			playdot();
			playdot();
		case '6':
			playdash();
			playdot();
			playdot();
			playdot();
			playdot();
			break;
		case '7':
			playdash();
			playdash();
			playdot();
			playdot();
			playdot();
			break;
		case '8':
			playdash();
			playdash();
			playdash();
			playdot();
			playdot();
			break;
		case '9':
			playdash();
			playdash();
			playdash();
			playdash();
			playdot();
			break;
		case '0':
			playdash();
			playdash();
			playdash();
			playdash();		
			playdash();
			break;
		case ' ':
			msleep_interruptible(dotlength*4);
			return;
		default:
			printk(KERN_NOTICE "Character '%c' not recognized\n", letter);
			return;
		}
		LETTERSLEEP;
	return;
}

static void playdot(void)
{
	unsigned char value;	
	unsigned long spinlock_flags;
	outb(CONTROLL_CODE_UPDATE_COUNTDOWN, PIT2_CONTROLL_PORT);
	smp_mb();
	outb(lowbyte, COUNTDOWN_UPDATE_PORT);
	smp_mb();
	outb(highbyte, COUNTDOWN_UPDATE_PORT);
	smp_mb();
	raw_spin_lock_irqsave(&i8253_lock, spinlock_flags);
	value=inb(ON_OFF_PORT);
	smp_mb();
	value=value | 3;
	smp_mb();
	outb(value, ON_OFF_PORT);
	raw_spin_unlock_irqrestore(&i8253_lock, spinlock_flags);
	msleep_interruptible(dotlength);
	raw_spin_lock_irqsave(&i8253_lock, spinlock_flags);
	value=inb(ON_OFF_PORT);
	smp_mb();
	value=value & 252;
	smp_mb();
	outb(value, ON_OFF_PORT);
	raw_spin_unlock_irqrestore(&i8253_lock, spinlock_flags);
	DOTSLEEP;
}

static void playdash(void)
{
	unsigned char value;
	unsigned long spinlock_flags;
	outb(CONTROLL_CODE_UPDATE_COUNTDOWN, PIT2_CONTROLL_PORT);
	smp_mb();
	outb(lowbyte, COUNTDOWN_UPDATE_PORT);
	smp_mb();
	outb(highbyte, COUNTDOWN_UPDATE_PORT);
	raw_spin_lock_irqsave(&i8253_lock, spinlock_flags);
	value=inb(ON_OFF_PORT);
	smp_mb();
	value=value | 3;
	smp_mb();
	outb(value, ON_OFF_PORT);
	raw_spin_unlock_irqrestore(&i8253_lock, spinlock_flags);
	msleep_interruptible(dotlength*3);
	raw_spin_lock_irqsave(&i8253_lock, spinlock_flags);
	value=inb(ON_OFF_PORT);
	smp_mb();
	value=value & 252;
	smp_mb();
	outb(value, ON_OFF_PORT);
	raw_spin_unlock_irqrestore(&i8253_lock, spinlock_flags);
	DOTSLEEP;
}
