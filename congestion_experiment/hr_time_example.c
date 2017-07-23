#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

unsigned long timer_interval_ns = 1e6;
struct hrtimer hr_timer;


enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart ){
  	ktime_t currtime , interval;
  	currtime  = ktime_get();
  	interval = ktime_set(0,timer_interval_ns);
  	hrtimer_forward(timer_for_restart, currtime , interval);
	// set_pin_value(PIO_G,9,(cnt++ & 1)); //Toggle LED

    //proven that this works in nanosec
    printk("timer_callback is runng\n");

	return HRTIMER_RESTART; //return HRTIMER_NORESTART;
}




static int __init hrtimer_test_init(void){
  ktime_t ktime;
  printk("BEGIN hrtimer\n");

  ktime = ktime_set( 0, timer_interval_ns );

	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &timer_callback;
 	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );

return 0;

}

static void __exit hrtimer_test_exit(void){
  int ret;
  	ret = hrtimer_cancel( &hr_timer );
  	if (ret) printk("The timer was still in use...\n");
  	printk("HR Timer module uninstalling\n");
}

module_init(hrtimer_test_init);
module_exit(hrtimer_test_exit);


MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Tesing hr_timer");
