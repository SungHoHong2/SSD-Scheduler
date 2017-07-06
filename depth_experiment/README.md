#### Generic Block Layer
- The generic block layer is an abstraction for block devices[HDD, SSD] in the system
- These block devices may be physical or logical
- Receives I/O requests in a queue, and is responsible for passing them along to block devices
- The I/O requests are scheduled to be submitted to the devices by an algorithm called an I/O scheduler


#### I/O Schedulers
- maximize the block i/O performance
- schedulers operate on pending requests
  - Merge requests for adjacent sectors
  - Sort pending requests based on LBA
  - Time when requests are submitted
  - Must minimize latency, maximize throughput, and avoid starvation


#### Analysis of Elevator-Framework
 - elevator.h has the interface functions that are overlapped with the customized-schedulers
 - elevator.c uses the interface functions within the elevator's key functions

#### - SET_REQUEST
 1. check whether elevator set function exists
    - if exists return the request function
    - if not return 0

```
int elv_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
	struct elevator_queue *e = q->elevator;
	if (e->type->ops.elevator_set_req_fn)
		return e->type->ops.elevator_set_req_fn(q, rq, bio, gfp_mask);
	return 0;
}
```

#### - ADD_REQUEST
1. secure mutex lock
2. invoke another elv_add_request

```
void elv_add_request(struct request_queue *q, struct request *rq, int where){
	unsigned long flags;
	spin_lock_irqsave(q->queue_lock, flags);
	__elv_add_request(q, rq, where);
	spin_unlock_irqrestore(q->queue_lock, flags);
}
EXPORT_SYMBOL(elv_add_request);

```

```
void __elv_add_request(struct request_queue *q, struct request *rq, int where){
	trace_block_rq_insert(q, rq);
	blk_pm_add_request(q, rq);
	rq->q = q;

  // check the type of the add request the scheduler performs
	if (rq->cmd_flags & REQ_SOFTBARRIER) {
		/* barriers are scheduling boundary, update end_sector */
		if (rq->cmd_type == REQ_TYPE_FS) {
			q->end_sector = rq_end_sector(rq);
			q->boundary_rq = rq;
		}
	} else if (!(rq->cmd_flags & REQ_ELVPRIV) &&
		    (where == ELEVATOR_INSERT_SORT ||
		     where == ELEVATOR_INSERT_SORT_MERGE))
		where = ELEVATOR_INSERT_BACK;

	switch (where) {
        case ELEVATOR_INSERT_FRONT:
      		rq->cmd_flags |= REQ_SOFTBARRIER;
      		list_add(&rq->queuelist, &q->queue_head);
      		break;

      	case ELEVATOR_INSERT_BACK:
      		rq->cmd_flags |= REQ_SOFTBARRIER;
      		elv_drain_elevator(q);
      		list_add_tail(&rq->queuelist, &q->queue_head);
      		/*
      		 * We kick the queue here for the following reasons.
      		 * - The elevator might have returned NULL previously
      		 *   to delay requests and returned them now.  As the
      		 *   queue wasn't empty before this request, ll_rw_blk
      		 *   won't run the queue on return, resulting in hang.
      		 * - Usually, back inserted requests won't be merged
      		 *   with anything.  There's no point in delaying queue
      		 *   processing.
      		 */
      		__blk_run_queue(q);
      		break;

        case ELEVATOR_INSERT_SORT:
      		BUG_ON(rq->cmd_type != REQ_TYPE_FS);
      		rq->cmd_flags |= REQ_SORTED;
      		q->nr_sorted++;
      		if (rq_mergeable(rq)) {
      			elv_rqhash_add(q, rq);
      			if (!q->last_merge)
      				q->last_merge = rq;
      		}

      		/*
      		 * Some ioscheds (cfq) run q->request_fn directly, so
      		 * rq cannot be accessed after calling
      		 * elevator_add_req_fn.
      		 */
      		q->elevator->type->ops.elevator_add_req_fn(q, rq);
      		break;
  }
```

#### - DISPATCH_REQUEST

```
void elv_drain_elevator(struct request_queue *q){
	static int printed;
	lockdep_assert_held(q->queue_lock);

	while (q->elevator->type->ops.elevator_dispatch_fn(q, 1))
		;
	if (q->nr_sorted && printed++ < 10) {
		printk(KERN_ERR "%s: forced dispatching is broken "
		       "(nr_sorted=%u), please report this\n",
		       q->elevator->type->elevator_name, q->nr_sorted);
	}
}
```


#### - COMPLETE_REQUEST

```
void elv_completed_request(struct request_queue *q, struct request *rq){
	struct elevator_queue *e = q->elevator;

	/*
	 * request is released from the driver, io must be done
	 */
	if (blk_account_rq(rq)) {
		q->in_flight[rq_is_sync(rq)]--;
		if ((rq->cmd_flags & REQ_SORTED) &&
		    e->type->ops.elevator_completed_req_fn)
			e->type->ops.elevator_completed_req_fn(q, rq);
	}
}
```

#### - PUT_REQUEST

```
void elv_put_request(struct request_queue *q, struct request *rq){
	struct elevator_queue *e = q->elevator;

	if (e->type->ops.elevator_put_req_fn)
		e->type->ops.elevator_put_req_fn(rq);
}
```




#### Analysis of CFQ Scheduler
- Per-process sorted queues for sync requests
- Fewer queues for async requests
- Allocates time slice for each queue
- Priorities are taken into account
- may idle if quantum not expired - anticipation
