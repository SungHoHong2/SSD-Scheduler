
struct cfq_data {
  struct hrtimer idle_slice_timer;
}

static enum hrtimer_restart cfq_idle_slice_timer(struct hrtimer *timer){
  unsigned long flags;
  struct cfq_data *cfqd = container_of(timer, struct cfq_data, idle_slice_timer);
  spin_lock_irqsave(cfqd->queue->queue_lock, flags);
  if (!RB_EMPTY_ROOT(&cfqq->sort_list))
    goto out_kick;
  out_kick:
  cfq_schedule_dispatch(cfqd);
  spin_unlock_irqrestore(cfqd->queue->queue_lock, flags);
}


static int cfq_init_queue(struct request_queue *q, struct elevator_type *e){
  hrtimer_init(&cfqd->idle_slice_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  cfqd->idle_slice_timer.function = cfq_idle_slice_timer;
}


static int cfq_dispatch_requests(struct request_queue *q, int force){
  cfqq = cfq_select_queue(cfqd);
	if (!cfqq)
		return 0;

	if (!cfq_dispatch_request(cfqd, cfqq))
		return 0;
}


static struct cfq_queue *cfq_select_queue(struct cfq_data *cfqd){
	struct cfq_queue *cfqq, *new_cfqq = NULL;
	u64 now = ktime_get_ns();

  cfqq = cfqd->active_queue;
	if (!cfqq)
		goto new_queue;

	if (!RB_EMPTY_ROOT(&cfqq->sort_list))
		goto keep_queue;

	if (hrtimer_active(&cfqd->idle_slice_timer)) {
		cfqq = NULL;
		goto keep_queue;
	}

  new_queue:
  cfqq = cfq_set_active_queue(cfqd, new_cfqq);

  keep_queue:
  	return cfqq;
}


static void cfq_arm_slice_timer(struct cfq_data *cfqd){
	if (cfqq->dispatched)
		return;
	hrtimer_start(&cfqd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
}


static void cfq_completed_request(struct request_queue *q, struct request *rq){

  /*
   * Idling is not enabled on:
   * - expired queues
   * - idle-priority queues
   * - async queues
   * - queues with still some requests queued
   * - when there is a close cooperator
   */
  if (cfq_slice_used(cfqq) || cfq_class_idle(cfqq))
    cfq_slice_expired(cfqd, 1);
  else if (sync && cfqq_empty &&
     !cfq_close_cooperator(cfqd, cfqq)) {
    cfq_arm_slice_timer(cfqd);
  }
}




static struct cfq_queue *cfq_set_active_queue(struct cfq_data *cfqd, struct cfq_queue *cfqq){
	if (!cfqq)
		cfqq = cfq_get_next_queue(cfqd);

	__cfq_set_active_queue(cfqd, cfqq);
	return cfqq;
}

static void __cfq_set_active_queue(struct cfq_data *cfqd, struct cfq_queue *cfqq){
  cfq_del_timer(cfqd, cfqq);
}

static inline void cfq_del_timer(struct cfq_data *cfqd, struct cfq_queue *cfqq){
	hrtimer_try_to_cancel(&cfqd->idle_slice_timer);
	cfqg_stats_update_idle_time(cfqq->cfqg);
}



static void cfq_shutdown_timer_wq(struct cfq_data *cfqd){
	hrtimer_cancel(&cfqd->idle_slice_timer);
	cancel_work_sync(&cfqd->unplug_work);
}

static void cfq_exit_queue(struct elevator_queue *e){
  cfq_shutdown_timer_wq(cfqd);
}
