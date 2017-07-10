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


 #define RQ_CIC(rq)		icq_to_cic((rq)->elv.icq)
 #define RQ_CFQQ(rq)		(struct cfq_queue *) ((rq)->elv.priv[0])
 #define RQ_CFQG(rq)		(struct cfq_group *) ((rq)->elv.priv[1])

 static struct kmem_cache *cfq_pool;

 #define CFQ_PRIO_LISTS		IOPRIO_BE_NR
 #define cfq_class_idle(cfqq)	((cfqq)->ioprio_class == IOPRIO_CLASS_IDLE)
 #define cfq_class_rt(cfqq)	((cfqq)->ioprio_class == IOPRIO_CLASS_RT)

 #define sample_valid(samples)	((samples) > 80)
 #define rb_entry_cfqg(node)	rb_entry((node), struct cfq_group, rb_node)

 /* blkio-related constants */
 #define CFQ_WEIGHT_LEGACY_MIN	10
 #define CFQ_WEIGHT_LEGACY_DFL	500
 #define CFQ_WEIGHT_LEGACY_MAX	1000

 struct cfq_ttime {
 	u64 last_end_request;

 	u64 ttime_total;
 	u64 ttime_mean;
 	unsigned long ttime_samples;
 };

 /*
  * Most of our rbtree usage is for sorting with min extraction, so
  * if we cache the leftmost node we don't have to walk down the tree
  * to find it. Idea borrowed from Ingo Molnars CFS scheduler. We should
  * move this into the elevator for the rq sorting as well.
  */
 struct cfq_rb_root {
 	struct rb_root rb;
 	struct rb_node *left;
 	unsigned count;
 	u64 min_vdisktime;
 	struct cfq_ttime ttime;
 };
 #define CFQ_RB_ROOT	(struct cfq_rb_root) { .rb = RB_ROOT, \
 			.ttime = {.last_end_request = ktime_get_ns(),},}


    /*
     * First index in the service_trees.
     * IDLE is handled separately, so it has negative index
     */
    enum wl_class_t {
    	BE_WORKLOAD = 0,
    	RT_WORKLOAD = 1,
    	IDLE_WORKLOAD = 2,
    	CFQ_PRIO_NR,
    };

    /*
     * Second index in the service_trees.
     */
    enum wl_type_t {
    	ASYNC_WORKLOAD = 0,
    	SYNC_NOIDLE_WORKLOAD = 1,
    	SYNC_WORKLOAD = 2
    };

    struct cfqg_stats {
    };


    /* Per-cgroup data */
    struct cfq_group_data {
    	/* must be the first member */
    	struct blkcg_policy_data cpd;

    	unsigned int weight;
    	unsigned int leaf_weight;
    };

    /* This is per cgroup per device grouping structure */
    struct cfq_group {
    	/* must be the first member */
    	struct blkg_policy_data pd;

    	/* group service_tree member */
    	struct rb_node rb_node;

    	/* group service_tree key */
    	u64 vdisktime;

    	int nr_active;
    	unsigned int children_weight;

    	unsigned int vfraction;

    	unsigned int weight;
    	unsigned int new_weight;
    	unsigned int dev_weight;

    	unsigned int leaf_weight;
    	unsigned int new_leaf_weight;
    	unsigned int dev_leaf_weight;

    	/* number of cfqq currently on this group */
    	int nr_cfqq;

    	unsigned int busy_queues_avg[CFQ_PRIO_NR];

    	struct cfq_rb_root service_trees[2][3];
    	struct cfq_rb_root service_tree_idle;

    	u64 saved_wl_slice;
    	enum wl_type_t saved_wl_type;
    	enum wl_class_t saved_wl_class;

    	/* number of requests that are on the dispatch list or inside driver */
    	int dispatched;
    	struct cfq_ttime ttime;
    	struct cfqg_stats stats;	/* stats for this cfqg */

    	/* async queue for each priority case */
    	struct cfq_queue *async_cfqq[2][IOPRIO_BE_NR];
    	struct cfq_queue *async_idle_cfqq;

    };



    /* Traverses through cfq group service trees */
    #define for_each_cfqg_st(cfqg, i, j, st) \
    	for (i = 0; i <= IDLE_WORKLOAD; i++) \
    		for (j = 0, st = i < IDLE_WORKLOAD ? &cfqg->service_trees[i][j]\
    			: &cfqg->service_tree_idle; \
    			(i < IDLE_WORKLOAD && j <= SYNC_WORKLOAD) || \
    			(i == IDLE_WORKLOAD && j == 0); \
    			j++, st = i < IDLE_WORKLOAD ? \
    			&cfqg->service_trees[i][j]: NULL) \




    /**
     * cfq_init_cfqg_base - initialize base part of a cfq_group
     * @cfqg: cfq_group to initialize
     *
     * Initialize the base part which is used whether %CONFIG_CFQ_GROUP_IOSCHED
     * is enabled or not.
     */
    static void cfq_init_cfqg_base(struct cfq_group *cfqg)
    {
    	struct cfq_rb_root *st;
    	int i, j;

    	for_each_cfqg_st(cfqg, i, j, st)
    		*st = CFQ_RB_ROOT;
    	RB_CLEAR_NODE(&cfqg->rb_node);

    	cfqg->ttime.last_end_request = ktime_get_ns();
    }


    struct cfq_queue {
     /* reference count */
     int ref;

    /* various state flags, see below */
    unsigned int flags;


     pid_t pid;

    /* parent cfq_data */
   	struct cfq_data *cfqd;

    /* service_tree member */
    struct rb_node rb_node;

    /* prio tree member */
    struct rb_node p_node;


    /* fifo list of requests in sort_list */
   	struct list_head fifo;

     /* io prio of this group */
   	unsigned short ioprio, org_ioprio;
   	unsigned short ioprio_class, org_ioprio_class;

    };



  /*
   * Per block device queue structure
   */
  struct cfq_data {

    struct request_queue *queue;
    struct cfq_rb_root grp_service_tree;
  	struct cfq_group *root_group;

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


    /*
  	 * Each priority tree is sorted by next_request position.  These
  	 * trees are used when determining if two or more queues are
  	 * interleaving requests (see cfq_close_cooperator).
  	 */
    struct rb_root prio_trees[CFQ_PRIO_LISTS];


		// FRISK
		struct list_head frisk_queue;
	 	int depth;
	 	struct cfq_request *curr_request;


    /*
  	 * Fallback dummy cfqq for extreme OOM conditions
  	 */
  	struct cfq_queue oom_cfqq;

  };


  enum cfqq_state_flags {
    CFQ_CFQQ_FLAG_on_rr = 0,	/* on round-robin busy list */
    CFQ_CFQQ_FLAG_wait_request,	/* waiting for a request */
    CFQ_CFQQ_FLAG_must_dispatch,	/* must be allowed a dispatch */
    CFQ_CFQQ_FLAG_must_alloc_slice,	/* per-slice must_alloc flag */
    CFQ_CFQQ_FLAG_fifo_expire,	/* FIFO checked in this slice */
    CFQ_CFQQ_FLAG_idle_window,	/* slice idling enabled */
    CFQ_CFQQ_FLAG_prio_changed,	/* task priority has changed */
    CFQ_CFQQ_FLAG_slice_new,	/* no requests dispatched in slice */
    CFQ_CFQQ_FLAG_sync,		/* synchronous queue */
    CFQ_CFQQ_FLAG_coop,		/* cfqq is shared */
    CFQ_CFQQ_FLAG_split_coop,	/* shared cfqq will be splitted */
    CFQ_CFQQ_FLAG_deep,		/* sync cfqq experienced large depth */
    CFQ_CFQQ_FLAG_wait_busy,	/* Waiting for next request */
  };



  #define CFQ_CFQQ_FNS(name)						\
  static inline void cfq_mark_cfqq_##name(struct cfq_queue *cfqq)		\
  {									\
    (cfqq)->flags |= (1 << CFQ_CFQQ_FLAG_##name);			\
  }									\
  static inline void cfq_clear_cfqq_##name(struct cfq_queue *cfqq)	\
  {									\
    (cfqq)->flags &= ~(1 << CFQ_CFQQ_FLAG_##name);			\
  }									\
  static inline int cfq_cfqq_##name(const struct cfq_queue *cfqq)		\
  {									\
    return ((cfqq)->flags & (1 << CFQ_CFQQ_FLAG_##name)) != 0;	\
  }

  // CFQ_CFQQ_FNS(on_rr);
  // CFQ_CFQQ_FNS(wait_request);
  // CFQ_CFQQ_FNS(must_dispatch);
  // CFQ_CFQQ_FNS(must_alloc_slice);
  CFQ_CFQQ_FNS(fifo_expire);
  CFQ_CFQQ_FNS(idle_window);
  CFQ_CFQQ_FNS(prio_changed);
  // CFQ_CFQQ_FNS(slice_new);
  CFQ_CFQQ_FNS(sync);
  // CFQ_CFQQ_FNS(coop);
  // CFQ_CFQQ_FNS(split_coop);
  // CFQ_CFQQ_FNS(deep);
  // CFQ_CFQQ_FNS(wait_busy);
  #undef CFQ_CFQQ_FNS



  static void cfq_init_cfqq(struct cfq_data *cfqd, struct cfq_queue *cfqq,
  			  pid_t pid, bool is_sync)
  {
  	RB_CLEAR_NODE(&cfqq->rb_node);
  	RB_CLEAR_NODE(&cfqq->p_node);
  	INIT_LIST_HEAD(&cfqq->fifo);

  	cfqq->ref = 0;
  	cfqq->cfqd = cfqd;

  	cfq_mark_cfqq_prio_changed(cfqq);

  	if (is_sync) {
  		if (!cfq_class_idle(cfqq))
  			cfq_mark_cfqq_idle_window(cfqq);
  		cfq_mark_cfqq_sync(cfqq);
  	}
  	cfqq->pid = pid;
  }



	// FRISK
	struct cfq_request{
		pid_t pid;
		int complete_flag;
	  struct list_head lists;
	};



static void cfq_completed(struct request_queue *q, struct request *rq){
	struct cfq_data *nd = q->elevator->elevator_data;
  nd->depth--;
}

static int cfq_dispatch(struct request_queue *q, int force){
	struct cfq_data *cfqd = q->elevator->elevator_data;
	struct request *rq;


  	rq = list_first_entry_or_null(&cfqd->frisk_queue, struct request, queuelist);
  	if (rq) {

      printk("TEST cfq_quantum: %d \n",cfqd->cfq_quantum);


      cfqd->depth++;
  		list_del_init(&rq->queuelist);
  		elv_dispatch_sort(q, rq);
  		return 1;
  	}
	return 0;
}

static void cfq_add_request(struct request_queue *q, struct request *rq){
	struct cfq_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->frisk_queue);
}


static int cfq_init_queue(struct request_queue *q, struct elevator_type *e){

	struct cfq_data *cfqd;
	int i, ret;
	struct elevator_queue *eq;

  // allocate elevator object
	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	// allocate cfq-global object
	cfqd = kmalloc_node(sizeof(*cfqd), GFP_KERNEL, q->node);

  // kill the kernel object if it fails
	if (!cfqd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}

  // assign cfq-global object to elevator_data
	eq->elevator_data = cfqd;
	cfqd->queue = q;

  // FRISK
	cfqd->depth = 0;
	INIT_LIST_HEAD(&cfqd->frisk_queue);
  // END FRISK

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);


  /* create rb_root */
	cfqd->grp_service_tree = CFQ_RB_ROOT;
  //  #define CFQ_RB_ROOT	(struct cfq_rb_root) {
  //    .rb = RB_ROOT,
  //	 	 .ttime = {.last_end_request = ktime_get_ns(),},}

  ret = -ENOMEM;

  // create rb_node
	cfqd->root_group = kzalloc_node(sizeof(*cfqd->root_group),
					GFP_KERNEL, cfqd->queue->node);
	if (!cfqd->root_group)
		goto out_free;

  // initialize rb_node
	cfq_init_cfqg_base(cfqd->root_group);

  //give weight to rb_node
	cfqd->root_group->weight = 2 * CFQ_WEIGHT_LEGACY_DFL;
	cfqd->root_group->leaf_weight = 2 * CFQ_WEIGHT_LEGACY_DFL;

  // give priority
  for (i = 0; i < CFQ_PRIO_LISTS; i++)
		cfqd->prio_trees[i] = RB_ROOT;


  cfq_init_cfqq(cfqd, &cfqd->oom_cfqq, 1, 0);
  cfqd->oom_cfqq.ref++;


  printk("CFQ EXPERIMENT\n");
	return 0;


  out_free:
  	kfree(cfqd);
  	kobject_put(&eq->kobj);
  	return ret;
}

static void cfq_exit_queue(struct elevator_queue *e)
{
	struct cfq_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->frisk_queue));
	kfree(nd);
}





/*
 * CFQ ATTRIBUTES
 */

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
		.elevator_completed_req_fn  = cfq_completed,
		.elevator_dispatch_fn		= cfq_dispatch,
		.elevator_add_req_fn		= cfq_add_request,
		.elevator_init_fn		= cfq_init_queue,
		.elevator_exit_fn		= cfq_exit_queue,
	},
	.elevator_attrs =	cfq_attrs,
	.elevator_name = "cfq_test",
	.elevator_owner = THIS_MODULE,
};

static int __init cfq_init(void){
	return elv_register(&elevator_noop);
}

static void __exit cfq_exit(void){
	elv_unregister(&elevator_noop);
}

module_init(cfq_init);
module_exit(cfq_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
