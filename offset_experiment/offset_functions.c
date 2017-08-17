/*
 * CFQ example
 */



static int
cfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio,
		gfp_t gfp_mask)
{
	struct cfq_data *cfqd = q->elevator->elevator_data;
	struct cfq_io_cq *cic = icq_to_cic(rq->elv.icq);
	const int rw = rq_data_dir(rq);
	const bool is_sync = rq_is_sync(rq);
	struct cfq_queue *cfqq;

	spin_lock_irq(q->queue_lock);

	check_ioprio_changed(cic, bio);
	check_blkcg_changed(cic, bio);
new_queue:
	cfqq = cic_to_cfqq(cic, is_sync);
	if (!cfqq || cfqq == &cfqd->oom_cfqq) {
		if (cfqq)
			cfq_put_queue(cfqq);
		cfqq = cfq_get_queue(cfqd, is_sync, cic, bio);
		cic_set_cfqq(cic, cfqq, is_sync);
	} else {
		/*
		 * If the queue was seeky for too long, break it apart.
		 */
		if (cfq_cfqq_coop(cfqq) && cfq_cfqq_split_coop(cfqq)) {
			cfq_log_cfqq(cfqd, cfqq, "breaking apart cfqq");
			cfqq = split_cfqq(cic, cfqq);
			if (!cfqq)
				goto new_queue;
		}

		/*
		 * Check to see if this queue is scheduled to merge with
		 * another, closely cooperating queue.  The merging of
		 * queues happens here as it must be done in process context.
		 * The reference on new_cfqq was taken in merge_cfqqs.
		 */
		if (cfqq->new_cfqq)
			cfqq = cfq_merge_cfqqs(cfqd, cic, cfqq);
	}

	cfqq->allocated[rw]++;

	cfqq->ref++;
	cfqg_get(cfqq->cfqg);
	rq->elv.priv[0] = cfqq;
	rq->elv.priv[1] = cfqq->cfqg;
	spin_unlock_irq(q->queue_lock);
	return 0;
}



static struct cfq_queue *
cfq_get_queue(struct cfq_data *cfqd, bool is_sync, struct cfq_io_cq *cic,
	      struct bio *bio)
{
	int ioprio_class = IOPRIO_PRIO_CLASS(cic->ioprio);
	int ioprio = IOPRIO_PRIO_DATA(cic->ioprio);
	struct cfq_queue **async_cfqq = NULL;
	struct cfq_queue *cfqq;
	struct cfq_group *cfqg;

	rcu_read_lock();
	cfqg = cfq_lookup_cfqg(cfqd, bio_blkcg(bio));
	if (!cfqg) {
		cfqq = &cfqd->oom_cfqq;
		goto out;
	}

	if (!is_sync) {
		if (!ioprio_valid(cic->ioprio)) {
			struct task_struct *tsk = current;
			ioprio = task_nice_ioprio(tsk);
			ioprio_class = task_nice_ioclass(tsk);
		}
		async_cfqq = cfq_async_queue_prio(cfqg, ioprio_class, ioprio);
		cfqq = *async_cfqq;
		if (cfqq)
			goto out;
	}



  static inline struct blkcg *bio_blkcg(struct bio *bio)
  {
  	if (bio && bio->bi_css)
  		return css_to_blkcg(bio->bi_css);
  	return task_blkcg(current);
  }


  static struct cfq_group *cfq_lookup_cfqg(struct cfq_data *cfqd,
					 struct blkcg *blkcg)
{
	struct blkcg_gq *blkg;

	blkg = blkg_lookup(blkcg, cfqd->queue);
	if (likely(blkg))
		return blkg_to_cfqg(blkg);
	return NULL;
}



/*
 * Deadline
 */


 static int
 deadline_merge(struct request_queue *q, struct request **req, struct bio *bio)
 {
 	struct deadline_data *dd = q->elevator->elevator_data;
 	struct request *__rq;
 	int ret;

 	/*
 	 * check for front merge
 	 */
 	if (dd->front_merges) {
 		sector_t sector = bio_end_sector(bio);

 		__rq = elv_rb_find(&dd->sort_list[bio_data_dir(bio)], sector);
 		if (__rq) {
 			BUG_ON(sector != blk_rq_pos(__rq));

 			if (elv_bio_merge_ok(__rq, bio)) {
 				ret = ELEVATOR_FRONT_MERGE;
 				goto out;
 			}
 		}
 	}

 	return ELEVATOR_NO_MERGE;
 out:
 	*req = __rq;
 	return ret;
 }



 struct request *elv_rb_find(struct rb_root *root, sector_t sector)
{
	struct rb_node *n = root->rb_node;
	struct request *rq;

	while (n) {
		rq = rb_entry(n, struct request, rb_node);

		if (sector < blk_rq_pos(rq))
			n = n->rb_left;
		else if (sector > blk_rq_pos(rq))
			n = n->rb_right;
		else
			return rq;
	}

	return NULL;
}
