#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include "morze_enter.h"

#define MOUSE_WAIT -1
#define MOUSE_PRESS 1
#define MOUSE_RELEASE 0

#define DOT_DASH_TIME_BORDER HZ / 3
#define WAIT_TIME HZ

static char last_mouse_status = MOUSE_WAIT;
static unsigned long stamp = 0;
static char entered_code[MAX_MORZE_CODE_LEN + 1] = "\0\0\0\0\0\0";
static int entered_code_len = 0;
static char entered_code_tasklet[MAX_MORZE_CODE_LEN + 1];

static struct tasklet_struct my_tasklet;
static struct timer_list my_timer;

static struct file *f_input_event;
struct input_event event_kbd;

void my_tasklet_function(unsigned long data);
void timer_check(struct timer_list *timer);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grishin E.B");
MODULE_DESCRIPTION("Entering by mouse morze");


static int __init morze_init( void )
{
    printk(KERN_INFO "[morze] in init\n");
    tasklet_init(&my_tasklet, my_tasklet_function, 0);
    timer_setup(&my_timer, timer_check, 0);
    f_input_event = filp_open("/dev/input/event3", O_WRONLY, 0);

    return 0;
}

static void __exit morze_exit( void )
{
    printk(KERN_INFO "[morze] in exit\n");
    tasklet_kill(&my_tasklet);
    del_timer(&my_timer);
    filp_close(f_input_event, current->files);
}


void my_tasklet_function(unsigned long data)
{
    int i = 0;
    for (; i < MORZE_DICT_LEN && strcmp(morze_codes[i], entered_code_tasklet); i++);
    if (i < MORZE_DICT_LEN)
    {
        printk(KERN_INFO "[morze] entered code: %s; entered symbol: %c\n", entered_code_tasklet, morze_symbols[i]);

        event_kbd.type = EV_KEY;
        event_kbd.code = morze_keys[i];
        event_kbd.value = 1;
        kernel_write(f_input_event, (char*)&event_kbd, sizeof(event_kbd), &f_input_event->f_pos);
        event_kbd.type = EV_KEY;
        event_kbd.code = morze_keys[i];;
        event_kbd.value = 0;
        kernel_write(f_input_event, (char*)&event_kbd, sizeof(event_kbd), &f_input_event->f_pos);
        event_kbd.type = EV_SYN;
        event_kbd.code = 0;
        event_kbd.value = 0;
        kernel_write(f_input_event, (char*)&event_kbd, sizeof(event_kbd), &f_input_event->f_pos);
    }
}

void timer_check(struct timer_list *timer)
{
    last_mouse_status = MOUSE_WAIT;
    strcpy(entered_code_tasklet, entered_code);
    tasklet_schedule(&my_tasklet);
}

extern void my_mouse_handler(char new_mouse_status)
{
    if (last_mouse_status == MOUSE_WAIT && new_mouse_status == MOUSE_RELEASE ||
        last_mouse_status == new_mouse_status ||
        new_mouse_status != MOUSE_RELEASE && new_mouse_status != MOUSE_PRESS)
        return;

    del_timer_sync(&my_timer);
    if (last_mouse_status == MOUSE_WAIT && new_mouse_status == MOUSE_PRESS)
    {
        entered_code_len = 0;
        memset(entered_code, '\0', MAX_MORZE_CODE_LEN);
        stamp = jiffies;
        last_mouse_status = MOUSE_PRESS;
        mod_timer(&my_timer, jiffies + WAIT_TIME);
    }
    else if (last_mouse_status == MOUSE_PRESS && new_mouse_status == MOUSE_RELEASE)
    {
        if ((long)jiffies - (long)stamp < DOT_DASH_TIME_BORDER)
            entered_code[entered_code_len] = '.';
        else
            entered_code[entered_code_len] = '-';
        entered_code_len++;
        if (entered_code_len == MAX_MORZE_CODE_LEN)
        {
            last_mouse_status = MOUSE_WAIT;
            strcpy(entered_code_tasklet, entered_code);
            tasklet_schedule(&my_tasklet);
        }
        else
        {
            last_mouse_status = MOUSE_RELEASE;
            mod_timer(&my_timer, jiffies + WAIT_TIME);
        }
    }
    else // if (last_mouse_status == MOUSE_RELEASE && new_mouse_status == MOUSE_PRESS)
    {
        stamp = jiffies;
        mod_timer(&my_timer, jiffies + WAIT_TIME);
        last_mouse_status = MOUSE_PRESS;
    }
}


EXPORT_SYMBOL(my_mouse_handler);

module_init(morze_init);
module_exit(morze_exit);
