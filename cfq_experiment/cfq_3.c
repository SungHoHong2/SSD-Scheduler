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
      struct blkg_stat		idle_time;
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
    u32 seek_history;


    /* parent cfq_data */
   	struct cfq_data *cfqd;

    /* service_tree member */
    struct rb_node rb_node;

    /* prio tree member */
    struct rb_node p_node;


    /* fifo list of requests in sort_list */
   	struct list_head fifo;


    /* time when first request from queue completed and slice started. */
  	u64 slice_start;
  	u64 slice_end;
  	s64 slice_resid;


     /* io prio of this group */
   	unsigned short ioprio, org_ioprio;
   	unsigned short ioprio_class, org_ioprio_class;
    struct cfq_group *cfqg;


    /* sorted list of pending requests */
  	struct rb_root sort_list;

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



    unsigned int busy_queues;


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



  // static inline int cfq_group_busy_queues_wl(enum wl_class_t wl_class,
  // 					struct cfq_data *cfqd,
  // 					struct cfq_group *cfqg)
  // {
  // 	if (wl_class == IDLE_WORKLOAD)
  // 		return cfqg->service_tree_idle.count;
  //
  // 	return cfqg->service_trees[wl_class][ASYNC_WORKLOAD].count +
  // 		cfqg->service_trees[wl_class][SYNC_NOIDLE_WORKLOAD].count +
  // 		cfqg->service_trees[wl_class][SYNC_WORKLOAD].count;
  // }

  /*
   * get averaged number of queues of RT/BE priority.
   * average is updated, with a formula that gives more weight to higher numbers,
   * to quickly follows sudden increases and decrease slowly
   */

  // static inline unsigned cfq_group_get_avg_queues(struct cfq_data *cfqd,
  // 					struct cfq_group *cfqg, bool rt)
  // {
  // 	unsigned min_q, max_q;
  // 	unsigned mult  = cfq_hist_divisor - 1;
  // 	unsigned round = cfq_hist_divisor / 2;
  // 	unsigned busy = cfq_group_busy_queues_wl(rt, cfqd, cfqg);
  //
  // 	min_q = min(cfqg->busy_queues_avg[rt], busy);
  // 	max_q = max(cfqg->busy_queues_avg[rt], busy);
  // 	cfqg->busy_queues_avg[rt] = (mult * max_q + min_q + round) /
  // 		cfq_hist_divisor;
  // 	return cfqg->busy_queues_avg[rt];
  // }


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


  static inline u64
  cfq_scaled_cfqq_slice(struct cfq_data *cfqd, struct cfq_queue *cfqq)
  {
  	u64 slice = cfq_prio_to_slice(cfqd, cfqq);
  	// if (cfqd->cfq_latency) {
  	// 	/*
  	// 	 * interested queues (we consider only the ones with the same
  	// 	 * priority class in the cfq group)
  	// 	 */
  	// 	unsigned iq = cfq_group_get_avg_queues(cfqd, cfqq->cfqg,
  	// 					cfq_class_rt(cfqq));
  	// 	u64 sync_slice = cfqd->cfq_slice[1];
  	// 	u64 expect_latency = sync_slice * iq;
  	// 	u64 group_slice = cfq_group_slice(cfqd, cfqq->cfqg);
    //
  	// 	if (expect_latency > group_slice) {
  	// 		u64 base_low_slice = 2 * cfqd->cfq_slice_idle;
  	// 		u64 low_slice;
    //
  	// 		/* scale low_slice according to IO priority
  	// 		 * and sync vs async */
  	// 		low_slice = div64_u64(base_low_slice*slice, sync_slice);
  	// 		low_slice = min(slice, low_slice);
  	// 		/* the adapted slice value is scaled to fit all iqs
  	// 		 * into the target latency */
  	// 		slice = div64_u64(slice*group_slice, expect_latency);
  	// 		slice = max(slice, low_slice);
  	// 	}
  	// }
  	return slice;
  }

  static void cfq_group_served(struct cfq_data *cfqd, struct cfq_group *cfqg,
  				struct cfq_queue *cfqq)
  {
  	// struct cfq_rb_root *st = &cfqd->grp_service_tree;
  	// u64 used_sl, charge, unaccounted_sl = 0;
  	// int nr_sync = cfqg->nr_cfqq - cfqg_busy_async_queues(cfqd, cfqg)
  	// 		- cfqg->service_tree_idle.count;
  	// unsigned int vfr;
  	// u64 now = ktime_get_ns();
    //
  	// BUG_ON(nr_sync < 0);
  	// used_sl = charge = cfq_cfqq_slice_usage(cfqq, &unaccounted_sl);
    //
  	// if (iops_mode(cfqd))
  	// 	charge = cfqq->slice_dispatch;
  	// else if (!cfq_cfqq_sync(cfqq) && !nr_sync)
  	// 	charge = cfqq->allocated_slice;
    //
  	// /*
  	//  * Can't update vdisktime while on service tree and cfqg->vfraction
  	//  * is valid only while on it.  Cache vfr, leave the service tree,
  	//  * update vdisktime and go back on.  The re-addition to the tree
  	//  * will also update the weights as necessary.
  	//  */
  	// vfr = cfqg->vfraction;
  	// cfq_group_service_tree_del(st, cfqg);
  	// cfqg->vdisktime += cfqg_scale_charge(charge, vfr);
  	// cfq_group_service_tree_add(st, cfqg);
    //
  	// /* This group is being expired. Save the context */
  	// if (cfqd->workload_expires > now) {
  	// 	cfqg->saved_wl_slice = cfqd->workload_expires - now;
  	// 	cfqg->saved_wl_type = cfqd->serving_wl_type;
  	// 	cfqg->saved_wl_class = cfqd->serving_wl_class;
  	// } else
  	// 	cfqg->saved_wl_slice = 0;
    //
  	// cfq_log_cfqg(cfqd, cfqg, "served: vt=%llu min_vt=%llu", cfqg->vdisktime,
  	// 				st->min_vdisktime);
  	// cfq_log_cfqq(cfqq->cfqd, cfqq,
  	// 	     "sl_used=%llu disp=%llu charge=%llu iops=%u sect=%lu",
  	// 	     used_sl, cfqq->slice_dispatch, charge,
  	// 	     iops_mode(cfqd), cfqq->nr_sectors);
  	// cfqg_stats_update_timeslice_used(cfqg, used_sl, unaccounted_sl);
  	// cfqg_stats_set_start_empty_time(cfqg);
  }



  /*
   * scheduler run of queue, if there are requests pending and no one in the
   * driver that will restart queueing
   */
  static inline void cfq_schedule_dispatch(struct cfq_data *cfqd)
  {
  	if (cfqd->busy_queues) {
  		kblockd_schedule_work(&cfqd->unplug_work);
  	}
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

  	// if (cfq_cfqq_on_rr(cfqq) && RB_EMPTY_ROOT(&cfqq->sort_list))
  	//	cfq_del_cfqq_rr(cfqd, cfqq);

  	// cfq_resort_rr_list(cfqd, cfqq);

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

  	cfqq = cfqd->active_queue;
  	if (cfqq) {
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
  	return HRTIMER_NORESTART;
  }



  static inline void cfq_link_cfqq_cfqg(struct cfq_queue *cfqq, struct cfq_group *cfqg) {
  	cfqq->cfqg = cfqg;
  }


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

      // printk("TEST cfq_quantum: %d \n",cfqd->cfq_quantum);


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


   spin_lock_irq(q->queue_lock);
	 cfq_link_cfqq_cfqg(&cfqd->oom_cfqq, cfqd->root_group);

  // cfqg_put(cfqd->root_group); // ERROR POINT
	 spin_unlock_irq(q->queue_lock);
  //
  //
   hrtimer_init(&cfqd->idle_slice_timer, CLOCK_MONOTONIC,
	 	     HRTIMER_MODE_REL);

  cfqd->idle_slice_timer.function = cfq_idle_slice_timer;

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
