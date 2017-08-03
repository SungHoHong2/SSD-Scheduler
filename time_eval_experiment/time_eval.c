#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/delay.h>


static int __init noop_init(void)
{

  struct timeval start_time;
  struct timeval finish_time;

  unsigned long start_time_track;
  unsigned long end_time_track;


  do_gettimeofday(&start_time); //it is known as jiffies
  start_time_track = (u32)(start_time.tv_sec);
  // rtc_time_to_tm(start_time_track, &tm);
  printk("before %ld\n", start_time_track);

  msleep(20000); // 10000 -> gave 10 <- this was a bit faster than 10 seconds.

  do_gettimeofday(&finish_time);
  end_time_track = (u32)(finish_time.tv_sec);
  printk("after %ld\n", end_time_track);

  printk("difference %ld\n", end_time_track-start_time_track);

  return 0;
}

static void __exit noop_exit(void){}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
