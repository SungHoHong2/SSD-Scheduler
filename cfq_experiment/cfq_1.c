/*
 * elevator noop
   when the noop runs with 64jobs with 1 depth it freezes
	 when the noop runs with 32jobs with 1 depth it works
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <linux/ioprio.h>
#include <linux/blktrace_api.h>
#include <linux/blk-cgroup.h>
#include "blk.h"

/*
 * tunables
 */
 /* max queue in one round of service */
 static const int cfq_quantum = 8;
 static const u64 cfq_fifo_expire[2] = { NSEC_PER_SEC / 4, NSEC_PER_SEC / 8 };

 /* maximum backwards seek, in KiB */
 static const int cfq_back_max = 16 * 1024;

 /* penalty of a backwards seek */
 static const int cfq_back_penalty = 2;
 static const u64 cfq_slice_sync = NSEC_PER_SEC / 10;
 static u64 cfq_slice_async = NSEC_PER_SEC / 25;
 static const int cfq_slice_async_rq = 2;


 static u64 cfq_slice_idle = NSEC_PER_SEC / 125;

 /*
 slice_idle
 ----------
 This specifies how long CFQ should idle for next request on certain cfq queues
 (for sequential workloads) and service trees (for random workloads) before
 queue is expired and CFQ selects next queue to dispatch from.
 */

 static u64 cfq_group_idle = NSEC_PER_SEC / 125;
 static const u64 cfq_target_latency = (u64)NSEC_PER_SEC * 3/10; /* 300 ms */
 static const int cfq_hist_divisor = 4;

 /*
  * offset from end of service tree
  */
 #define CFQ_IDLE_DELAY		(NSEC_PER_SEC / 5)

 /*
  * below this threshold, we consider thinktime immediate
  */
 #define CFQ_MIN_TT		(2 * NSEC_PER_SEC / HZ)


 #define CFQ_SLICE_SCALE		(5)
 #define CFQ_HW_QUEUE_MIN	(5)
 #define CFQ_SERVICE_SHIFT       12

 #define CFQQ_SEEK_THR		(sector_t)(8 * 100)
 #define CFQQ_CLOSE_THR		(sector_t)(8 * 1024)
 #define CFQQ_SECT_THR_NONROT	(sector_t)(2 * 32)
 #define CFQQ_SEEKY(cfqq)	(hweight32(cfqq->seek_history) > 32/8)



  /*
   * Per block device queue structure
   */
  struct cfq_data {

  	/*
  	 * tunables, see top of file
  	 */
  	unsigned int cfq_quantum;
  	unsigned int cfq_back_penalty;
  	unsigned int cfq_back_max;
  	unsigned int cfq_slice_async_rq;
  	unsigned int cfq_latency;

		u64 cfq_fifo_expire[2];
		u64 cfq_slice[2];
		u64 cfq_slice_idle;
		u64 cfq_group_idle;
		u64 cfq_target_latency;

  };





#define REQUEST_DEPTH 10

struct noop_data {
	struct list_head queue;
	int depth;
	struct noop_request *curr_request;
};

struct noop_request{
	pid_t pid;
	int complete_flag;
  struct list_head lists;
};


static void noop_completed(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
  nd->depth--;
}

static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

    if(nd->depth<=REQUEST_DEPTH){

  	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
  	if (rq) {
      nd->depth++;
  		list_del_init(&rq->queuelist);
  		elv_dispatch_sort(q, rq);
  		return 1;
  	}
  }
	return 0;
}

static void noop_add_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->queue);
}


static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct noop_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}

	eq->elevator_data = nd;

	nd->depth = 0;
	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);


  printk("CFQ EXPERIMENT %d\n",REQUEST_DEPTH);
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}




static ssize_t
cfq_var_show(unsigned int var, char *page){
 return sprintf(page, "%u\n", var);
}

static ssize_t
cfq_var_store(unsigned int *var, const char *page, size_t count){
 char *p = (char *) page;

 *var = simple_strtoul(p, &p, 10);
 return count;
}


#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 u64 __data = __VAR;						\
 if (__CONV)							\
	 __data = div_u64(__data, NSEC_PER_MSEC);			\
 return cfq_var_show(__data, (page));				\
}

SHOW_FUNCTION(cfq_quantum_show, cfqd->cfq_quantum, 0);
SHOW_FUNCTION(cfq_fifo_expire_sync_show, cfqd->cfq_fifo_expire[1], 1);
SHOW_FUNCTION(cfq_fifo_expire_async_show, cfqd->cfq_fifo_expire[0], 1);
SHOW_FUNCTION(cfq_back_seek_max_show, cfqd->cfq_back_max, 0);
SHOW_FUNCTION(cfq_back_seek_penalty_show, cfqd->cfq_back_penalty, 0);
SHOW_FUNCTION(cfq_slice_idle_show, cfqd->cfq_slice_idle, 1);
SHOW_FUNCTION(cfq_group_idle_show, cfqd->cfq_group_idle, 1);
SHOW_FUNCTION(cfq_slice_sync_show, cfqd->cfq_slice[1], 1);
SHOW_FUNCTION(cfq_slice_async_show, cfqd->cfq_slice[0], 1);
SHOW_FUNCTION(cfq_slice_async_rq_show, cfqd->cfq_slice_async_rq, 0);
SHOW_FUNCTION(cfq_low_latency_show, cfqd->cfq_latency, 0);
SHOW_FUNCTION(cfq_target_latency_show, cfqd->cfq_target_latency, 1);
#undef SHOW_FUNCTION


#define USEC_SHOW_FUNCTION(__FUNC, __VAR)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct cfq_data *cfqd = e->elevator_data;			\
	u64 __data = __VAR;						\
	__data = div_u64(__data, NSEC_PER_USEC);			\
	return cfq_var_show(__data, (page));				\
}
USEC_SHOW_FUNCTION(cfq_slice_idle_us_show, cfqd->cfq_slice_idle);
USEC_SHOW_FUNCTION(cfq_group_idle_us_show, cfqd->cfq_group_idle);
USEC_SHOW_FUNCTION(cfq_slice_sync_us_show, cfqd->cfq_slice[1]);
USEC_SHOW_FUNCTION(cfq_slice_async_us_show, cfqd->cfq_slice[0]);
USEC_SHOW_FUNCTION(cfq_target_latency_us_show, cfqd->cfq_target_latency);
#undef USEC_SHOW_FUNCTION



#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 unsigned int __data;						\
 int ret = cfq_var_store(&__data, (page), count);		\
 if (__data < (MIN))						\
	 __data = (MIN);						\
 else if (__data > (MAX))					\
	 __data = (MAX);						\
 if (__CONV)							\
	 *(__PTR) = (u64)__data * NSEC_PER_MSEC;			\
 else								\
	 *(__PTR) = __data;					\
 return ret;							\
}

STORE_FUNCTION(cfq_quantum_store, &cfqd->cfq_quantum, 1, UINT_MAX, 0);
STORE_FUNCTION(cfq_fifo_expire_sync_store, &cfqd->cfq_fifo_expire[1], 1,
	 UINT_MAX, 1);
STORE_FUNCTION(cfq_fifo_expire_async_store, &cfqd->cfq_fifo_expire[0], 1,
	 UINT_MAX, 1);
STORE_FUNCTION(cfq_back_seek_max_store, &cfqd->cfq_back_max, 0, UINT_MAX, 0);
STORE_FUNCTION(cfq_back_seek_penalty_store, &cfqd->cfq_back_penalty, 1,
	 UINT_MAX, 0);
STORE_FUNCTION(cfq_slice_idle_store, &cfqd->cfq_slice_idle, 0, UINT_MAX, 1);
STORE_FUNCTION(cfq_group_idle_store, &cfqd->cfq_group_idle, 0, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_sync_store, &cfqd->cfq_slice[1], 1, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_async_store, &cfqd->cfq_slice[0], 1, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_async_rq_store, &cfqd->cfq_slice_async_rq, 1,
	 UINT_MAX, 0);
STORE_FUNCTION(cfq_low_latency_store, &cfqd->cfq_latency, 0, 1, 0);
STORE_FUNCTION(cfq_target_latency_store, &cfqd->cfq_target_latency, 1, UINT_MAX, 1);
#undef STORE_FUNCTION


#define USEC_STORE_FUNCTION(__FUNC, __PTR, MIN, MAX)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 unsigned int __data;						\
 int ret = cfq_var_store(&__data, (page), count);		\
 if (__data < (MIN))						\
	 __data = (MIN);						\
 else if (__data > (MAX))					\
	 __data = (MAX);						\
 *(__PTR) = (u64)__data * NSEC_PER_USEC;				\
 return ret;							\
}
USEC_STORE_FUNCTION(cfq_slice_idle_us_store, &cfqd->cfq_slice_idle, 0, UINT_MAX);
USEC_STORE_FUNCTION(cfq_group_idle_us_store, &cfqd->cfq_group_idle, 0, UINT_MAX);
USEC_STORE_FUNCTION(cfq_slice_sync_us_store, &cfqd->cfq_slice[1], 1, UINT_MAX);
USEC_STORE_FUNCTION(cfq_slice_async_us_store, &cfqd->cfq_slice[0], 1, UINT_MAX);
USEC_STORE_FUNCTION(cfq_target_latency_us_store, &cfqd->cfq_target_latency, 1, UINT_MAX);
#undef USEC_STORE_FUNCTION


#define CFQ_ATTR(name) \
	__ATTR(name, S_IRUGO|S_IWUSR, cfq_##name##_show, cfq_##name##_store)

static struct elv_fs_entry cfq_attrs[] = {
	CFQ_ATTR(quantum),
	CFQ_ATTR(fifo_expire_sync),
	CFQ_ATTR(fifo_expire_async),
	CFQ_ATTR(back_seek_max),
	CFQ_ATTR(back_seek_penalty),
	CFQ_ATTR(slice_sync),
	CFQ_ATTR(slice_sync_us),
	CFQ_ATTR(slice_async),
	CFQ_ATTR(slice_async_us),
	CFQ_ATTR(slice_async_rq),
	CFQ_ATTR(slice_idle),
	CFQ_ATTR(slice_idle_us),
	CFQ_ATTR(group_idle),
	CFQ_ATTR(group_idle_us),
	CFQ_ATTR(low_latency),
	CFQ_ATTR(target_latency),
	CFQ_ATTR(target_latency_us),
	__ATTR_NULL
};


static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_completed_req_fn  = noop_completed,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_attrs =	cfq_attrs,
	.elevator_name = "cfq_test",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
