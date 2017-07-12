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


 static struct blkcg_policy blkcg_policy_cfq;

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
      struct blkg_stat		idle_time;
      struct blkg_stat		time;
      struct blkg_stat		dequeue;

      uint64_t			start_group_wait_time;
      uint64_t			start_idle_time;
      uint64_t			start_empty_time;
      uint16_t			flags;
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

      printk("7-1 initialize the base part which is used or not\n");

    	for_each_cfqg_st(cfqg, i, j, st)
    		*st = CFQ_RB_ROOT;
    	RB_CLEAR_NODE(&cfqg->rb_node);

      printk("7-2 check number of loops for each cfq_groups %d %d\n", i, j);

    	cfqg->ttime.last_end_request = ktime_get_ns();
    }




    struct cfq_queue {
     /* reference count */
     int ref;

    /* various state flags, see below */
    unsigned int flags;


    pid_t pid;
    u32 seek_history;

    u64 rb_key;

    /* parent cfq_data */
   	struct cfq_data *cfqd;

    /* service_tree member */
    struct rb_node rb_node;

    /* prio tree member */
    struct rb_node p_node;


    /* fifo list of requests in sort_list */
   	struct list_head fifo;

    /* if fifo isn't expired, next request to serve */
    struct request *next_rq;

    /* time when queue got scheduled in to dispatch first request. */
  	u64 dispatch_start;
  	u64 allocated_slice;
  	u64 slice_dispatch;
  	/* time when first request from queue completed and slice started. */
  	u64 slice_start;
  	u64 slice_end;
  	s64 slice_resid;


     /* io prio of this group */
   	unsigned short ioprio, org_ioprio;
   	unsigned short ioprio_class, org_ioprio_class;
    struct cfq_group *cfqg;



    /* prio tree root we belong to, if any */
  	struct rb_root *p_root;

    /* sorted list of pending requests */
  	struct rb_root sort_list;


    struct cfq_rb_root *service_tree;


    };


    struct cfq_io_cq {
    	struct io_cq		icq;		/* must be the first member */
    	struct cfq_queue	*cfqq[2];
    	struct cfq_ttime	ttime;
    	int			ioprio;		/* the current ioprio */

    };


  /*
   * Per block device queue structure
   */
  struct cfq_data {

    struct request_queue *queue;
    struct cfq_rb_root grp_service_tree;
  	struct cfq_group *root_group;


    /*
  	 * The priority currently being served
  	 */
  	enum wl_class_t serving_wl_class;
  	enum wl_type_t serving_wl_type;
  	u64 workload_expires;
  	struct cfq_group *serving_group;


    unsigned int busy_queues;
    unsigned int busy_sync_queues;

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

    struct hrtimer idle_slice_timer;
    struct work_struct unplug_work;
    struct cfq_io_cq *active_cic;


    int hw_tag;



		// FRISK
		struct list_head frisk_queue;
	 	int depth;
	 	struct cfq_request *curr_request;


    /*
  	 * Fallback dummy cfqq for extreme OOM conditions
  	 */

    struct cfq_queue *active_queue;
  	struct cfq_queue oom_cfqq;
    u64 last_delayed_sync;

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

  CFQ_CFQQ_FNS(on_rr);
  CFQ_CFQQ_FNS(wait_request);
  CFQ_CFQQ_FNS(must_dispatch);
  // CFQ_CFQQ_FNS(must_alloc_slice);
  CFQ_CFQQ_FNS(fifo_expire);
  CFQ_CFQQ_FNS(idle_window);
  CFQ_CFQQ_FNS(prio_changed);
  CFQ_CFQQ_FNS(slice_new);
  CFQ_CFQQ_FNS(sync);
  CFQ_CFQQ_FNS(coop);
  CFQ_CFQQ_FNS(split_coop);
  CFQ_CFQQ_FNS(deep);
  CFQ_CFQQ_FNS(wait_busy);
  #undef CFQ_CFQQ_FNS


  /* cfqg stats flags */
  enum cfqg_stats_flags {
  	CFQG_stats_waiting = 0,
  	CFQG_stats_idling,
  	CFQG_stats_empty,
  };

  #define CFQG_FLAG_FNS(name)						\
  static inline void cfqg_stats_mark_##name(struct cfqg_stats *stats)	\
  {									\
  	stats->flags |= (1 << CFQG_stats_##name);			\
  }									\
  static inline void cfqg_stats_clear_##name(struct cfqg_stats *stats)	\
  {									\
  	stats->flags &= ~(1 << CFQG_stats_##name);			\
  }									\
  static inline int cfqg_stats_##name(struct cfqg_stats *stats)		\
  {									\
  	return (stats->flags & (1 << CFQG_stats_##name)) != 0;		\
  }									\

  CFQG_FLAG_FNS(waiting)
  CFQG_FLAG_FNS(idling)
  CFQG_FLAG_FNS(empty)
  #undef CFQG_FLAG_FNS



  static inline enum wl_class_t cfqq_class(struct cfq_queue *cfqq)
  {
  	if (cfq_class_idle(cfqq))
  		return IDLE_WORKLOAD;
  	if (cfq_class_rt(cfqq))
  		return RT_WORKLOAD;
  	return BE_WORKLOAD;
  }


  static enum wl_type_t cfqq_type(struct cfq_queue *cfqq)
  {
  	if (!cfq_cfqq_sync(cfqq))
  		return ASYNC_WORKLOAD;
  	if (!cfq_cfqq_idle_window(cfqq))
  		return SYNC_NOIDLE_WORKLOAD;
  	return SYNC_WORKLOAD;
  }


  static struct cfq_rb_root *st_for(struct cfq_group *cfqg,
  					    enum wl_class_t class,
  					    enum wl_type_t type)
  {
  	if (!cfqg)
  		return NULL;

  	if (class == IDLE_WORKLOAD)
  		return &cfqg->service_tree_idle;

  	return &cfqg->service_trees[class][type];
  }


  /*
   * scheduler run of queue, if there are requests pending and no one in the
   * driver that will restart queueing
   */

    static inline void cfq_schedule_dispatch(struct cfq_data *cfqd){
    //	if (cfqd->busy_queues) {
        int ret;
        if(cfqd->depth>0){
    		    ret = kblockd_schedule_work(&cfqd->unplug_work);
        }
        printk("---<<< schedule dispatch %d >>>>---\n", ret);

    //	}
    }



  static inline int cfq_group_busy_queues_wl(enum wl_class_t wl_class,
  					struct cfq_data *cfqd,
  					struct cfq_group *cfqg)
  {
  	if (wl_class == IDLE_WORKLOAD)
  		return cfqg->service_tree_idle.count;

  	return cfqg->service_trees[wl_class][ASYNC_WORKLOAD].count +
  		cfqg->service_trees[wl_class][SYNC_NOIDLE_WORKLOAD].count +
  		cfqg->service_trees[wl_class][SYNC_WORKLOAD].count;
  }

  /*
   * get averaged number of queues of RT/BE priority.
   * average is updated, with a formula that gives more weight to higher numbers,
   * to quickly follows sudden increases and decrease slowly
   */

  static inline unsigned cfq_group_get_avg_queues(struct cfq_data *cfqd,
  					struct cfq_group *cfqg, bool rt)
  {
  	unsigned min_q, max_q;
  	unsigned mult  = cfq_hist_divisor - 1;
  	unsigned round = cfq_hist_divisor / 2;
  	unsigned busy = cfq_group_busy_queues_wl(rt, cfqd, cfqg);

  	min_q = min(cfqg->busy_queues_avg[rt], busy);
  	max_q = max(cfqg->busy_queues_avg[rt], busy);
  	cfqg->busy_queues_avg[rt] = (mult * max_q + min_q + round) /
  		cfq_hist_divisor;
  	return cfqg->busy_queues_avg[rt];
  }


  /*
   * Scale schedule slice based on io priority. Use the sync time slice only
   * if a queue is marked sync and has sync io queued. A sync queue with async
   * io only, should not get full sync slice length.
   */
  static inline u64 cfq_prio_slice(struct cfq_data *cfqd, bool sync,
  				 unsigned short prio)
  {
  	u64 base_slice = cfqd->cfq_slice[sync];
  	u64 slice = div_u64(base_slice, CFQ_SLICE_SCALE);

  	WARN_ON(prio >= IOPRIO_BE_NR);

  	return base_slice + (slice * (4 - prio));
  }

  static inline u64
  cfq_prio_to_slice(struct cfq_data *cfqd, struct cfq_queue *cfqq){
  	return cfq_prio_slice(cfqd, cfq_cfqq_sync(cfqq), cfqq->ioprio);
  }

  static inline int cfqg_busy_async_queues(struct cfq_data *cfqd,
  					struct cfq_group *cfqg){
  	return cfqg->service_trees[RT_WORKLOAD][ASYNC_WORKLOAD].count +
  		cfqg->service_trees[BE_WORKLOAD][ASYNC_WORKLOAD].count;
  }


  static inline u64
  cfq_group_slice(struct cfq_data *cfqd, struct cfq_group *cfqg)
  {
  	return cfqd->cfq_target_latency * cfqg->vfraction >> CFQ_SERVICE_SHIFT;
  }


  static inline u64
  cfq_scaled_cfqq_slice(struct cfq_data *cfqd, struct cfq_queue *cfqq){
  	u64 slice = cfq_prio_to_slice(cfqd, cfqq);
  	if (cfqd->cfq_latency) {
  		/*
  		 * interested queues (we consider only the ones with the same
  		 * priority class in the cfq group)
  		 */
  		unsigned iq = cfq_group_get_avg_queues(cfqd, cfqq->cfqg,
  						cfq_class_rt(cfqq));
  		u64 sync_slice = cfqd->cfq_slice[1];
  		u64 expect_latency = sync_slice * iq;
  		u64 group_slice = cfq_group_slice(cfqd, cfqq->cfqg);

  		if (expect_latency > group_slice) {
  			u64 base_low_slice = 2 * cfqd->cfq_slice_idle;
  			u64 low_slice;

  			/* scale low_slice according to IO priority
  			 * and sync vs async */
  			low_slice = div64_u64(base_low_slice*slice, sync_slice);
  			low_slice = min(slice, low_slice);
  			/* the adapted slice value is scaled to fit all iqs
  			 * into the target latency */
  			slice = div64_u64(slice*group_slice, expect_latency);
  			slice = max(slice, low_slice);
  		}
  	}
  	return slice;
  }


  static inline u64 cfq_cfqq_slice_usage(struct cfq_queue *cfqq,
  				       u64 *unaccounted_time)
  {
  	u64 slice_used;
  	u64 now = ktime_get_ns();

  	/*
  	 * Queue got expired before even a single request completed or
  	 * got expired immediately after first request completion.
  	 */
  	if (!cfqq->slice_start || cfqq->slice_start == now) {
  		/*
  		 * Also charge the seek time incurred to the group, otherwise
  		 * if there are mutiple queues in the group, each can dispatch
  		 * a single request on seeky media and cause lots of seek time
  		 * and group will never know it.
  		 */
  		slice_used = max_t(u64, (now - cfqq->dispatch_start),
  					jiffies_to_nsecs(1));
  	} else {
  		slice_used = now - cfqq->slice_start;
  		if (slice_used > cfqq->allocated_slice) {
  			*unaccounted_time = slice_used - cfqq->allocated_slice;
  			slice_used = cfqq->allocated_slice;
  		}
  		if (cfqq->slice_start > cfqq->dispatch_start)
  			*unaccounted_time += cfqq->slice_start -
  					cfqq->dispatch_start;
  	}

  	return slice_used;
  }


  static inline bool iops_mode(struct cfq_data *cfqd){
  	/*
  	 * If we are not idling on queues and it is a NCQ drive, parallel
  	 * execution of requests is on and measuring time is not possible
  	 * in most of the cases until and unless we drive shallower queue
  	 * depths and that becomes a performance bottleneck. In such cases
  	 * switch to start providing fairness in terms of number of IOs.
  	 */
  	if (!cfqd->cfq_slice_idle && cfqd->hw_tag)
  		return true;
  	else
  		return false;
  }

  static void rb_erase_init(struct rb_node *n, struct rb_root *root)
  {
  	rb_erase(n, root);
  	RB_CLEAR_NODE(n);
  }

  static void cfq_rb_erase(struct rb_node *n, struct cfq_rb_root *root){
  	if (root->left == n)
  		root->left = NULL;
  	rb_erase_init(n, &root->rb);
  	--root->count;
  }

  static inline struct cfq_group *pd_to_cfqg(struct blkg_policy_data *pd)
  {
  	return pd ? container_of(pd, struct cfq_group, pd) : NULL;
  }


  static inline struct blkcg_gq *cfqg_to_blkg(struct cfq_group *cfqg)
  {
  	return pd_to_blkg(&cfqg->pd);
  }


  static inline struct cfq_group *blkg_to_cfqg(struct blkcg_gq *blkg)
  {
  	return pd_to_cfqg(blkg_to_pd(blkg, &blkcg_policy_cfq));
  }


  static inline struct cfq_queue *cic_to_cfqq(struct cfq_io_cq *cic, bool is_sync)
  {
  	return cic->cfqq[is_sync];
  }

  static inline void cic_set_cfqq(struct cfq_io_cq *cic, struct cfq_queue *cfqq,
  				bool is_sync)
  {
  	cic->cfqq[is_sync] = cfqq;
  }

  static inline struct cfq_group *cfqg_parent(struct cfq_group *cfqg)
  {
  	struct blkcg_gq *pblkg = cfqg_to_blkg(cfqg)->parent;

  	return pblkg ? blkg_to_cfqg(pblkg) : NULL;
  }

  static inline s64
  cfqg_key(struct cfq_rb_root *st, struct cfq_group *cfqg)
  {
  	return cfqg->vdisktime - st->min_vdisktime;
  }




  static void
  __cfq_group_service_tree_add(struct cfq_rb_root *st, struct cfq_group *cfqg)
  {
  	struct rb_node **node = &st->rb.rb_node;
  	struct rb_node *parent = NULL;
  	struct cfq_group *__cfqg;
  	s64 key = cfqg_key(st, cfqg);
  	int left = 1;

  	while (*node != NULL) {
  		parent = *node;
  		__cfqg = rb_entry_cfqg(parent);

  		if (key < cfqg_key(st, __cfqg))
  			node = &parent->rb_left;
  		else {
  			node = &parent->rb_right;
  			left = 0;
  		}
  	}

  	if (left)
  		st->left = &cfqg->rb_node;

  	rb_link_node(&cfqg->rb_node, parent, node);
  	rb_insert_color(&cfqg->rb_node, &st->rb);
  }


  /*
   * This has to be called only on activation of cfqg
   */
  static void
  cfq_update_group_weight(struct cfq_group *cfqg)
  {
  	if (cfqg->new_weight) {
  		cfqg->weight = cfqg->new_weight;
  		cfqg->new_weight = 0;
  	}
  }


  static void cfq_update_group_leaf_weight(struct cfq_group *cfqg)
  {
  	BUG_ON(!RB_EMPTY_NODE(&cfqg->rb_node));

  	if (cfqg->new_leaf_weight) {
  		cfqg->leaf_weight = cfqg->new_leaf_weight;
  		cfqg->new_leaf_weight = 0;
  	}
  }




  static void
  cfq_group_service_tree_add(struct cfq_rb_root *st, struct cfq_group *cfqg)
  {
  	unsigned int vfr = 1 << CFQ_SERVICE_SHIFT;	/* start with 1 */
  	struct cfq_group *pos = cfqg;
  	struct cfq_group *parent;
  	bool propagate;

  	/* add to the service tree */
  	BUG_ON(!RB_EMPTY_NODE(&cfqg->rb_node));

  	/*
  	 * Update leaf_weight.  We cannot update weight at this point
  	 * because cfqg might already have been activated and is
  	 * contributing its current weight to the parent's child_weight.
  	 */
  	cfq_update_group_leaf_weight(cfqg);
  	__cfq_group_service_tree_add(st, cfqg);

  	/*
  	 * Activate @cfqg and calculate the portion of vfraction @cfqg is
  	 * entitled to.  vfraction is calculated by walking the tree
  	 * towards the root calculating the fraction it has at each level.
  	 * The compounded ratio is how much vfraction @cfqg owns.
  	 *
  	 * Start with the proportion tasks in this cfqg has against active
  	 * children cfqgs - its leaf_weight against children_weight.
  	 */
  	propagate = !pos->nr_active++;
  	pos->children_weight += pos->leaf_weight;
  	vfr = vfr * pos->leaf_weight / pos->children_weight;

  	/*
  	 * Compound ->weight walking up the tree.  Both activation and
  	 * vfraction calculation are done in the same loop.  Propagation
  	 * stops once an already activated node is met.  vfraction
  	 * calculation should always continue to the root.
  	 */
  	while ((parent = cfqg_parent(pos))) {
  		if (propagate) {
  			cfq_update_group_weight(pos);
  			propagate = !parent->nr_active++;
  			parent->children_weight += pos->weight;
  		}
  		vfr = vfr * pos->weight / parent->children_weight;
  		pos = parent;
  	}

  	cfqg->vfraction = max_t(unsigned, vfr, 1);
  }


  static void
  cfq_group_service_tree_del(struct cfq_rb_root *st, struct cfq_group *cfqg)
  {
  	struct cfq_group *pos = cfqg;
  	bool propagate;

  	/*
  	 * Undo activation from cfq_group_service_tree_add().  Deactivate
  	 * @cfqg and propagate deactivation upwards.
  	 */
  	propagate = !--pos->nr_active;
  	pos->children_weight -= pos->leaf_weight;

  	while (propagate) {
  		struct cfq_group *parent = cfqg_parent(pos);

  		/* @pos has 0 nr_active at this point */
  		WARN_ON_ONCE(pos->children_weight);
  		pos->vfraction = 0;

  		if (!parent)
  			break;

  		propagate = !--parent->nr_active;
  		parent->children_weight -= pos->weight;
  		pos = parent;
  	}

  	/* remove from the service tree */
  	if (!RB_EMPTY_NODE(&cfqg->rb_node))
  		cfq_rb_erase(&cfqg->rb_node, st);
  }

  static void cfqg_stats_update_dequeue(struct cfq_group *cfqg)
  {
  	blkg_stat_add(&cfqg->stats.dequeue, 1);
  }


  static void
  cfq_group_notify_queue_del(struct cfq_data *cfqd, struct cfq_group *cfqg)
  {
  	struct cfq_rb_root *st = &cfqd->grp_service_tree;

  	BUG_ON(cfqg->nr_cfqq < 1);
  	cfqg->nr_cfqq--;

  	/* If there are other cfq queues under this group, don't delete it */
  	if (cfqg->nr_cfqq)
  		return;

  	cfq_group_service_tree_del(st, cfqg);
  	cfqg->saved_wl_slice = 0;
  	cfqg_stats_update_dequeue(cfqg);
  }


  static inline u64 cfqg_scale_charge(u64 charge,
  				    unsigned int vfraction)
  {
  	u64 c = charge << CFQ_SERVICE_SHIFT;	/* make it fixed point */

  	/* charge / vfraction */
  	c <<= CFQ_SERVICE_SHIFT;
  	return div_u64(c, vfraction);
  }


  static inline void cfqg_stats_update_timeslice_used(struct cfq_group *cfqg,
  			uint64_t time, unsigned long unaccounted_time)
  {
  	blkg_stat_add(&cfqg->stats.time, time);
  }


  static void cfq_group_served(struct cfq_data *cfqd, struct cfq_group *cfqg,
  				struct cfq_queue *cfqq){

  	struct cfq_rb_root *st = &cfqd->grp_service_tree;
  	u64 used_sl, charge, unaccounted_sl = 0;
  	int nr_sync = cfqg->nr_cfqq - cfqg_busy_async_queues(cfqd, cfqg)
  			- cfqg->service_tree_idle.count;
  	unsigned int vfr;
  	u64 now = ktime_get_ns();

  	BUG_ON(nr_sync < 0);
  	used_sl = charge = cfq_cfqq_slice_usage(cfqq, &unaccounted_sl);

  	if (iops_mode(cfqd))
  		charge = cfqq->slice_dispatch;
  	else if (!cfq_cfqq_sync(cfqq) && !nr_sync)
  		charge = cfqq->allocated_slice;

  	/*
  	 * Can't update vdisktime while on service tree and cfqg->vfraction
  	 * is valid only while on it.  Cache vfr, leave the service tree,
  	 * update vdisktime and go back on.  The re-addition to the tree
  	 * will also update the weights as necessary.
  	 */

  	vfr = cfqg->vfraction;
  	cfq_group_service_tree_del(st, cfqg);
  	cfqg->vdisktime += cfqg_scale_charge(charge, vfr);
  	cfq_group_service_tree_add(st, cfqg);


    // CHARA
  	/* This group is being expired. Save the context */
  	if (cfqd->workload_expires > now) {
  		cfqg->saved_wl_slice = cfqd->workload_expires - now;
  		cfqg->saved_wl_type = cfqd->serving_wl_type;
  		cfqg->saved_wl_class = cfqd->serving_wl_class;
  	} else
  		cfqg->saved_wl_slice = 0;

  	cfqg_stats_update_timeslice_used(cfqg, used_sl, unaccounted_sl);
  	// cfqg_stats_set_start_empty_time(cfqg);
  }


  static inline struct cfq_io_cq *icq_to_cic(struct io_cq *icq){
  	/* cic->icq is the first member, %NULL will convert to %NULL */
  	return container_of(icq, struct cfq_io_cq, icq);
  }

  static inline struct cfq_data *cic_to_cfqd(struct cfq_io_cq *cic){
  	return cic->icq.q->elevator->elevator_data;
  }


  static void cfqg_stats_update_idle_time(struct cfq_group *cfqg){
  	struct cfqg_stats *stats = &cfqg->stats;

  	if (cfqg_stats_idling(stats)) {
  		unsigned long long now = sched_clock();

  		if (time_after64(now, stats->start_idle_time))
  			blkg_stat_add(&stats->idle_time,
  				      now - stats->start_idle_time);
  		cfqg_stats_clear_idling(stats);
  	}
  }


  static inline void cfq_del_timer(struct cfq_data *cfqd, struct cfq_queue *cfqq){
  	hrtimer_try_to_cancel(&cfqd->idle_slice_timer);
  	cfqg_stats_update_idle_time(cfqq->cfqg);
  }


  /*
   * The below is leftmost cache rbtree addon
   */
  static struct cfq_queue *cfq_rb_first(struct cfq_rb_root *root)
  {
  	/* Service tree is empty */
  	if (!root->count)
  		return NULL;

  	if (!root->left)
  		root->left = rb_first(&root->rb);

  	if (root->left)
  		return rb_entry(root->left, struct cfq_queue, rb_node);

  	return NULL;
  }


  /*
   * Called when the cfqq no longer has requests pending, remove it from
   * the service tree.
   */
  static void cfq_del_cfqq_rr(struct cfq_data *cfqd, struct cfq_queue *cfqq)
  {
  	BUG_ON(!cfq_cfqq_on_rr(cfqq));
  	cfq_clear_cfqq_on_rr(cfqq);

  	if (!RB_EMPTY_NODE(&cfqq->rb_node)) {
  		cfq_rb_erase(&cfqq->rb_node, cfqq->service_tree);
  		cfqq->service_tree = NULL;
  	}
  	if (cfqq->p_root) {
  		rb_erase(&cfqq->p_node, cfqq->p_root);
  		cfqq->p_root = NULL;
  	}

  	cfq_group_notify_queue_del(cfqd, cfqq->cfqg);
  	BUG_ON(!cfqd->busy_queues);
  	cfqd->busy_queues--;
  	if (cfq_cfqq_sync(cfqq))
  		cfqd->busy_sync_queues--;
  }



  static u64 cfq_slice_offset(struct cfq_data *cfqd,
  			    struct cfq_queue *cfqq)
  {
  	/*
  	 * just an approximation, should be ok.
  	 */
  	return (cfqq->cfqg->nr_cfqq - 1) * (cfq_prio_slice(cfqd, 1, 0) -
  		       cfq_prio_slice(cfqd, cfq_cfqq_sync(cfqq), cfqq->ioprio));
  }


  static struct cfq_queue *
  cfq_prio_tree_lookup(struct cfq_data *cfqd, struct rb_root *root,
  		     sector_t sector, struct rb_node **ret_parent,
  		     struct rb_node ***rb_link)
{
  	struct rb_node **p, *parent;
  	struct cfq_queue *cfqq = NULL;

  	parent = NULL;
  	p = &root->rb_node;
  	while (*p) {
  		struct rb_node **n;

  		parent = *p;
  		cfqq = rb_entry(parent, struct cfq_queue, p_node);

  		/*
  		 * Sort strictly based on sector.  Smallest to the left,
  		 * largest to the right.
  		 */
  		if (sector > blk_rq_pos(cfqq->next_rq))
  			n = &(*p)->rb_right;
  		else if (sector < blk_rq_pos(cfqq->next_rq))
  			n = &(*p)->rb_left;
  		else
  			break;
  		p = n;
  		cfqq = NULL;
  	}

  	*ret_parent = parent;
  	if (rb_link)
  		*rb_link = p;
  	return cfqq;
  }


  static void cfq_prio_tree_add(struct cfq_data *cfqd, struct cfq_queue *cfqq)
  {
  	struct rb_node **p, *parent;
  	struct cfq_queue *__cfqq;

  	if (cfqq->p_root) {
  		rb_erase(&cfqq->p_node, cfqq->p_root);
  		cfqq->p_root = NULL;
  	}

  	if (cfq_class_idle(cfqq))
  		return;
  	if (!cfqq->next_rq)
  		return;

  	cfqq->p_root = &cfqd->prio_trees[cfqq->org_ioprio];
  	__cfqq = cfq_prio_tree_lookup(cfqd, cfqq->p_root,
  				      blk_rq_pos(cfqq->next_rq), &parent, &p);
  	if (!__cfqq) {
  		rb_link_node(&cfqq->p_node, parent, p);
  		rb_insert_color(&cfqq->p_node, cfqq->p_root);
  	} else
  		cfqq->p_root = NULL;
  }



  static void
  cfq_group_notify_queue_add(struct cfq_data *cfqd, struct cfq_group *cfqg)
  {
  	struct cfq_rb_root *st = &cfqd->grp_service_tree;
  	struct cfq_group *__cfqg;
  	struct rb_node *n;

  	cfqg->nr_cfqq++;
  	if (!RB_EMPTY_NODE(&cfqg->rb_node))
  		return;

  	/*
  	 * Currently put the group at the end. Later implement something
  	 * so that groups get lesser vtime based on their weights, so that
  	 * if group does not loose all if it was not continuously backlogged.
  	 */
  	n = rb_last(&st->rb);
  	if (n) {
  		__cfqg = rb_entry_cfqg(n);
  		cfqg->vdisktime = __cfqg->vdisktime + CFQ_IDLE_DELAY;
  	} else
  		cfqg->vdisktime = st->min_vdisktime;
  	cfq_group_service_tree_add(st, cfqg);
  }


  /*
   * The cfqd->service_trees holds all pending cfq_queue's that have
   * requests waiting to be processed. It is sorted in the order that
   * we will service the queues.
   */
  static void cfq_service_tree_add(struct cfq_data *cfqd, struct cfq_queue *cfqq,
  				 bool add_front)
  {
  	struct rb_node **p, *parent;
  	struct cfq_queue *__cfqq;
  	u64 rb_key;
  	struct cfq_rb_root *st;
  	int left;
  	int new_cfqq = 1;
  	u64 now = ktime_get_ns();

  	st = st_for(cfqq->cfqg, cfqq_class(cfqq), cfqq_type(cfqq));
  	if (cfq_class_idle(cfqq)) {
  		rb_key = CFQ_IDLE_DELAY;
  		parent = rb_last(&st->rb);
  		if (parent && parent != &cfqq->rb_node) {
  			__cfqq = rb_entry(parent, struct cfq_queue, rb_node);
  			rb_key += __cfqq->rb_key;
  		} else
  			rb_key += now;
  	} else if (!add_front) {
  		/*
  		 * Get our rb key offset. Subtract any residual slice
  		 * value carried from last service. A negative resid
  		 * count indicates slice overrun, and this should position
  		 * the next service time further away in the tree.
  		 */
  		rb_key = cfq_slice_offset(cfqd, cfqq) + now;
  		rb_key -= cfqq->slice_resid;
  		cfqq->slice_resid = 0;
  	} else {
  		rb_key = -NSEC_PER_SEC;
  		__cfqq = cfq_rb_first(st);
  		rb_key += __cfqq ? __cfqq->rb_key : now;
  	}

  	if (!RB_EMPTY_NODE(&cfqq->rb_node)) {
  		new_cfqq = 0;
  		/*
  		 * same position, nothing more to do
  		 */
  		if (rb_key == cfqq->rb_key && cfqq->service_tree == st)
  			return;

  		cfq_rb_erase(&cfqq->rb_node, cfqq->service_tree);
  		cfqq->service_tree = NULL;
  	}

  	left = 1;
  	parent = NULL;
  	cfqq->service_tree = st;
  	p = &st->rb.rb_node;
  	while (*p) {
  		parent = *p;
  		__cfqq = rb_entry(parent, struct cfq_queue, rb_node);

  		/*
  		 * sort by key, that represents service time.
  		 */
  		if (rb_key < __cfqq->rb_key)
  			p = &parent->rb_left;
  		else {
  			p = &parent->rb_right;
  			left = 0;
  		}
  	}

  	if (left)
  		st->left = &cfqq->rb_node;

  	cfqq->rb_key = rb_key;
  	rb_link_node(&cfqq->rb_node, parent, p);
  	rb_insert_color(&cfqq->rb_node, &st->rb);
  	st->count++;
  	if (add_front || !new_cfqq)
  		return;
  	cfq_group_notify_queue_add(cfqd, cfqq->cfqg);
  }


  /*
   * Update cfqq's position in the service tree.
   */
  static void cfq_resort_rr_list(struct cfq_data *cfqd, struct cfq_queue *cfqq)
  {
  	/*
  	 * Resorting requires the cfqq to be on the RR list already.
  	 */
  	if (cfq_cfqq_on_rr(cfqq)) {
  		cfq_service_tree_add(cfqd, cfqq, 0);
  		cfq_prio_tree_add(cfqd, cfqq);
  	}
  }


  /*
   * current cfqq expired its slice (or was too idle), select new one
   */
  static void
  __cfq_slice_expired(struct cfq_data *cfqd, struct cfq_queue *cfqq,
  		    bool timed_out)
  {

  	if (cfq_cfqq_wait_request(cfqq))
  		cfq_del_timer(cfqd, cfqq);

  	cfq_clear_cfqq_wait_request(cfqq);
  	cfq_clear_cfqq_wait_busy(cfqq);

  	/*
  	 * If this cfqq is shared between multiple processes, check to
  	 * make sure that those processes are still issuing I/Os within
  	 * the mean seek distance.  If not, it may be time to break the
  	 * queues apart again.
  	 */
  	if (cfq_cfqq_coop(cfqq) && CFQQ_SEEKY(cfqq))
  		cfq_mark_cfqq_split_coop(cfqq);

  	/*
  	 * store what was left of this slice, if the queue idled/timed out
  	 */
  	if (timed_out) {
  		if (cfq_cfqq_slice_new(cfqq))
  			cfqq->slice_resid = cfq_scaled_cfqq_slice(cfqd, cfqq);
  		else
  			cfqq->slice_resid = cfqq->slice_end - ktime_get_ns();
  	}

  	cfq_group_served(cfqd, cfqq->cfqg, cfqq);

  	if (cfq_cfqq_on_rr(cfqq) && RB_EMPTY_ROOT(&cfqq->sort_list))
  		  cfq_del_cfqq_rr(cfqd, cfqq);

  	cfq_resort_rr_list(cfqd, cfqq);

  	if (cfqq == cfqd->active_queue)
  		cfqd->active_queue = NULL;

  	if (cfqd->active_cic) {
  		put_io_context(cfqd->active_cic->icq.ioc);
  		cfqd->active_cic = NULL;
  	}
  }



  static inline void cfq_slice_expired(struct cfq_data *cfqd, bool timed_out)
  {
  	struct cfq_queue *cfqq = cfqd->active_queue;

  	if (cfqq)
    __cfq_slice_expired(cfqd, cfqq, timed_out);
  }



  /*
   * We need to wrap this check in cfq_cfqq_slice_new(), since ->slice_end
   * isn't valid until the first request from the dispatch is activated
   * and the slice time set.
   */
  static inline bool cfq_slice_used(struct cfq_queue *cfqq){
  	if (cfq_cfqq_slice_new(cfqq))
  		return false;
  	if (ktime_get_ns() < cfqq->slice_end)
  		return false;

  	return true;
  }



  static void cfq_kick_queue(struct work_struct *work){
  	struct cfq_data *cfqd =
  		container_of(work, struct cfq_data, unplug_work);
  	struct request_queue *q = cfqd->queue;

  	spin_lock_irq(q->queue_lock);
  	__blk_run_queue(cfqd->queue);
  	spin_unlock_irq(q->queue_lock);
  }

  static inline void cfqg_put(struct cfq_group *cfqg) { }

  /*
   * Timer running if the active_queue is currently idling inside its time slice
   */
  static enum hrtimer_restart cfq_idle_slice_timer(struct hrtimer *timer){
  	struct cfq_data *cfqd = container_of(timer, struct cfq_data,
  					     idle_slice_timer);
  	struct cfq_queue *cfqq;
  	unsigned long flags;
  	int timed_out = 1;

  	spin_lock_irqsave(cfqd->queue->queue_lock, flags);

    printk("12-1. check if there is an active queue in cfq_data\n");
  	cfqq = cfqd->active_queue;
  	if (cfqq) {
      printk("12-2. active_queue exists\n");
  		timed_out = 0;

  		/*
  		 * We saw a request before the queue expired, let it through
  		 */
  		if (cfq_cfqq_must_dispatch(cfqq))
  			goto out_kick;

  		/*
  		 * expired
  		 */
  		if (cfq_slice_used(cfqq))
  			goto expire;

  		/*
  		 * only expire and reinvoke request handler, if there are
  		 * other queues with pending requests
  		 */
  		if (!cfqd->busy_queues)
  			goto out_cont;

  		/*
  		 * not expired and it has a request pending, let it dispatch
  		 */
  		if (!RB_EMPTY_ROOT(&cfqq->sort_list))
  			goto out_kick;

  		/*
  		 * Queue depth flag is reset only when the idle didn't succeed
  		 */
  		cfq_clear_cfqq_deep(cfqq);
  	}

  expire:
  	cfq_slice_expired(cfqd, timed_out);
  out_kick:
  	cfq_schedule_dispatch(cfqd);
  out_cont:
  	spin_unlock_irqrestore(cfqd->queue->queue_lock, flags);
    printk("12-4. reinvoke request handler\n");
  	return HRTIMER_NORESTART;
  }



  static inline void cfq_link_cfqq_cfqg(struct cfq_queue *cfqq, struct cfq_group *cfqg) {
  	cfqq->cfqg = cfqg;
  }


  static void cfq_init_cfqq(struct cfq_data *cfqd, struct cfq_queue *cfqq,
  			  pid_t pid, bool is_sync)
  {

    printk("10-1 clear rb_node of service_tree member \n");
  	RB_CLEAR_NODE(&cfqq->rb_node);
    printk("10-2 clear rb_node of priority tree member \n");
  	RB_CLEAR_NODE(&cfqq->p_node);
    printk("10-3 initialize fifo-list for requests \n");
  	INIT_LIST_HEAD(&cfqq->fifo);

    printk("10-4 initialize reference count (number of requests) \n");
  	cfqq->ref = 0;
  	cfqq->cfqd = cfqd;

    printk("10-5 assign cfq_queue flag for task priority change\n");
  	cfq_mark_cfqq_prio_changed(cfqq);

  	if (is_sync) {
      printk("10-6 check whether the last parameter is sync\n");
  		if (!cfq_class_idle(cfqq))
  			cfq_mark_cfqq_idle_window(cfqq);
  		cfq_mark_cfqq_sync(cfqq);
  	}

    printk("10-7 assign 1 to pid\n");
  	cfqq->pid = pid;
  }



	// FRISK
	struct cfq_request{
		pid_t pid;
		int complete_flag;
	  struct list_head lists;
	};



  static void cfq_activate_request(struct request_queue *q, struct request *rq){
     printk("----> cfq_activate_request\n");
  }


  static void cfq_deactivate_request(struct request_queue *q, struct request *rq){
     printk("----> cfq_deactivate_request\n");
  }

  static void cfq_put_request(struct request *rq){
     printk("----> cfq_put_request\n");
  }

  static int cfq_may_queue(struct request_queue *q, int op, int op_flags){
     printk("----> cfq_may_queue\n");
  	return ELV_MQUEUE_MAY;
  }

  static void cfq_registered_queue(struct request_queue *q){
     printk("----> cfq_registered_queue\n");
  }

static void cfq_completed_request(struct request_queue *q, struct request *rq){
	struct cfq_data *cfqd = q->elevator->elevator_data;
  printk("----> cfq_completed\n");

  cfq_schedule_dispatch(cfqd);
  cfqd->depth--;
}

static int cfq_dispatch_requests(struct request_queue *q, int force){
	struct cfq_data *cfqd = q->elevator->elevator_data;
	struct request *rq;

  	rq = list_first_entry_or_null(&cfqd->frisk_queue, struct request, queuelist);
  	if (rq) {

      // printk("TEST cfq_quantum: %d \n",cfqd->cfq_quantum);
      printk("----> cfq_dispatch\n");
      cfqd->depth++;
  		list_del_init(&rq->queuelist);
  		elv_dispatch_sort(q, rq);
  		return 1;
  	}
	return 0;
}

static void cfq_add_request(struct request_queue *q, struct request *rq){
	struct cfq_data *nd = q->elevator->elevator_data;
  printk("----> cfq_add_request\n");
	list_add_tail(&rq->queuelist, &nd->frisk_queue);
}

static int
cfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio,
		gfp_t gfp_mask){
  printk("----> cfq_set_request\n");
 return 0;
}


static void cfq_exit_cfqq(struct cfq_data *cfqd, struct cfq_queue *cfqq){
	if (unlikely(cfqq == cfqd->active_queue)) {
		__cfq_slice_expired(cfqd, cfqq, 0);
		cfq_schedule_dispatch(cfqd);
	}

  // CHARA
	// cfq_put_cooperator(cfqq);
	// cfq_put_queue(cfqq);
}


// CHARA
static void cfq_init_icq(struct io_cq *icq){
  struct cfq_io_cq *cic = icq_to_cic(icq);
  printk("----> cfq_init_icq\n");
  cic->ttime.last_end_request = ktime_get_ns();
}

static void cfq_exit_icq(struct io_cq *icq){

  struct cfq_io_cq *cic = icq_to_cic(icq);
  struct cfq_data *cfqd = cic_to_cfqd(cic);

  printk("----> cfq_exit_icq\n");

  if (cic_to_cfqq(cic, false)) {
		cfq_exit_cfqq(cfqd, cic_to_cfqq(cic, false));
		cic_set_cfqq(cic, NULL, false);
	}

  if (cic_to_cfqq(cic, true)) {
    cfq_exit_cfqq(cfqd, cic_to_cfqq(cic, true));
    cic_set_cfqq(cic, NULL, true);
  }

}


static int cfq_init_queue(struct request_queue *q, struct elevator_type *e){

	struct cfq_data *cfqd;
	int i, ret;
	struct elevator_queue *eq;

  printk("----> cfq_init_queue\n");
  printk("1. allocate elevator object\n");
	eq = elevator_alloc(q, e);

	if (!eq)
		return -ENOMEM;

	printk("2. allocate cfq-global object\n");
	cfqd = kmalloc_node(sizeof(*cfqd), GFP_KERNEL, q->node);

  printk("3. kill the kernel object if it fails\n");

	if (!cfqd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}

  printk("4. assign cfq-global object to elevator_data\n");
	eq->elevator_data = cfqd;
	cfqd->queue = q;

  // FRISK
	cfqd->depth = 0;
	INIT_LIST_HEAD(&cfqd->frisk_queue);
  // END FRISK

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("5. create rb_root\n");
	cfqd->grp_service_tree = CFQ_RB_ROOT;
  //  #define CFQ_RB_ROOT	(struct cfq_rb_root) {
  //    .rb = RB_ROOT,
  //	 	 .ttime = {.last_end_request = ktime_get_ns(),},}

  ret = -ENOMEM;

  printk("6. create rb_node\n");
	cfqd->root_group = kzalloc_node(sizeof(*cfqd->root_group),
					GFP_KERNEL, cfqd->queue->node);
	if (!cfqd->root_group)
		goto out_free;

  printk("7. initialize rb_node\n");

	cfq_init_cfqg_base(cfqd->root_group);

  printk("8. weight to rb_node\n");
	cfqd->root_group->weight = 2 * CFQ_WEIGHT_LEGACY_DFL;
	cfqd->root_group->leaf_weight = 2 * CFQ_WEIGHT_LEGACY_DFL;

  printk("9. give priority %d\n",CFQ_PRIO_LISTS);
  for (i = 0; i < CFQ_PRIO_LISTS; i++)
		cfqd->prio_trees[i] = RB_ROOT;

  printk("10. allocate fixed cfq_queue\n");
  cfq_init_cfqq(cfqd, &cfqd->oom_cfqq, 1, 0);
  cfqd->oom_cfqq.ref++;


  spin_lock_irq(q->queue_lock);
	cfq_link_cfqq_cfqg(&cfqd->oom_cfqq, cfqd->root_group);

  cfqg_put(cfqd->root_group);
	spin_unlock_irq(q->queue_lock);


  printk("11. initialize hrtimer\n");
  hrtimer_init(&cfqd->idle_slice_timer, CLOCK_MONOTONIC,
	 	     HRTIMER_MODE_REL);

  printk("12. initialize hrtimer's native function\n");
  cfqd->idle_slice_timer.function = cfq_idle_slice_timer;


  printk("13. initialize work_structn\n");
  INIT_WORK(&cfqd->unplug_work, cfq_kick_queue);


  cfqd->cfq_quantum = cfq_quantum;
	cfqd->cfq_fifo_expire[0] = cfq_fifo_expire[0];
	cfqd->cfq_fifo_expire[1] = cfq_fifo_expire[1];
	cfqd->cfq_back_max = cfq_back_max;
	cfqd->cfq_back_penalty = cfq_back_penalty;
	cfqd->cfq_slice[0] = cfq_slice_async;
	cfqd->cfq_slice[1] = cfq_slice_sync;
	cfqd->cfq_target_latency = cfq_target_latency;
	cfqd->cfq_slice_async_rq = cfq_slice_async_rq;
	cfqd->cfq_slice_idle = cfq_slice_idle;
	cfqd->cfq_group_idle = cfq_group_idle;
	cfqd->cfq_latency = 1;
	cfqd->hw_tag = -1;
  cfqd->last_delayed_sync = ktime_get_ns() - NSEC_PER_SEC;
  printk("CFQ EXPERIMENT NEOVERSION 001\n");
	return 0;


  out_free:
  	kfree(cfqd);
  	kobject_put(&eq->kobj);
  	return ret;
}


static void cfq_shutdown_timer_wq(struct cfq_data *cfqd){
	hrtimer_cancel(&cfqd->idle_slice_timer);
	cancel_work_sync(&cfqd->unplug_work);
}

static void cfq_exit_queue(struct elevator_queue *e){

	struct cfq_data *cfqd = e->elevator_data;
  struct request_queue *q = cfqd->queue;

  cfq_shutdown_timer_wq(cfqd);
  spin_lock_irq(q->queue_lock);

  if (cfqd->active_queue)
		__cfq_slice_expired(cfqd, cfqd->active_queue, 0);

  spin_unlock_irq(q->queue_lock);
  cfq_shutdown_timer_wq(cfqd);

  kfree(cfqd->root_group);
	BUG_ON(!list_empty(&cfqd->frisk_queue));
	kfree(cfqd);
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


static struct elevator_type elevator_cfq = {
	.ops = {
    // .elevator_merge_fn = 		cfq_merge,
		// .elevator_merged_fn =		cfq_merged_request,
		// .elevator_merge_req_fn =	cfq_merged_requests,
		// .elevator_allow_bio_merge_fn =	cfq_allow_bio_merge,
		// .elevator_allow_rq_merge_fn =	cfq_allow_rq_merge,
		// .elevator_bio_merged_fn =	cfq_bio_merged,
		.elevator_dispatch_fn =		cfq_dispatch_requests,
		.elevator_add_req_fn =		cfq_add_request,
		.elevator_activate_req_fn =	cfq_activate_request,
		.elevator_deactivate_req_fn =	cfq_deactivate_request,
		.elevator_completed_req_fn =	cfq_completed_request,
		.elevator_former_req_fn =	elv_rb_former_request,
		.elevator_latter_req_fn =	elv_rb_latter_request,
		.elevator_init_icq_fn =		cfq_init_icq,
		.elevator_exit_icq_fn =		cfq_exit_icq,
		.elevator_set_req_fn =		cfq_set_request,
		.elevator_put_req_fn =		cfq_put_request,
		.elevator_may_queue_fn =	cfq_may_queue,
		.elevator_init_fn =		cfq_init_queue,
		.elevator_exit_fn =		cfq_exit_queue,
		.elevator_registered_fn =	cfq_registered_queue,
	},
  .icq_size	=	sizeof(struct cfq_io_cq),
  .icq_align	=	__alignof__(struct cfq_io_cq),
	.elevator_attrs =	cfq_attrs,
	.elevator_name = "cfq_test",
	.elevator_owner = THIS_MODULE,
};

// FRISK PRINT
// 2294491,2418145 write /media/sf_SSD-Scheduler/cfq_experiment/cfq_test/cfq_test_08.txt

static int __init cfq_init(void){
  int ret;

  cfq_group_idle = 0;
  ret = -ENOMEM;

  printk("----> cfq_init\n");
  printk("1. add memory cache for cfq_queue\n");
  cfq_pool = KMEM_CACHE(cfq_queue, 0);

  if (!cfq_pool)
		goto err_pol_unreg;


  printk("2. register elevator\n");
  ret = elv_register(&elevator_cfq);

  if (ret) kmem_cache_destroy(cfq_pool);

err_pol_unreg:
  return ret;

}

static void __exit cfq_exit(void){
	elv_unregister(&elevator_cfq);
  kmem_cache_destroy(cfq_pool);
}

module_init(cfq_init);
module_exit(cfq_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
