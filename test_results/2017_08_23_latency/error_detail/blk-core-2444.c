void blk_dequeue_request(struct request *rq)
{
	struct request_queue *q = rq->q;

	BUG_ON(list_empty(&rq->queuelist));
	BUG_ON(ELV_ON_HASH(rq));

	list_del_init(&rq->queuelist);

	/*
	 * the time frame between a request being removed from the lists
	 * and to it is freed is accounted as io that is in progress at
	 * the driver side.
	 */
	if (blk_account_rq(rq)) {
		q->in_flight[rq_is_sync(rq)]++;
		set_io_start_time_ns(rq);
	}
}


void blk_start_request(struct request *req)
{
	blk_dequeue_request(req);

	/*
	 * We are now handing the request to the hardware, initialize
	 * resid_len to full count and add the timeout handler.
	 */
	req->resid_len = blk_rq_bytes(req);
	if (unlikely(blk_bidi_rq(req)))
		req->next_rq->resid_len = blk_rq_bytes(req->next_rq);

	BUG_ON(test_bit(REQ_ATOM_COMPLETE, &req->atomic_flags));
	blk_add_timer(req);
}
EXPORT_SYMBOL(blk_start_request);



int blk_queue_start_tag(struct request_queue *q, struct request *rq)
{
	struct blk_queue_tag *bqt = q->queue_tags;
	unsigned max_depth;
	int tag;

	if (unlikely((rq->cmd_flags & REQ_QUEUED))) {
		printk(KERN_ERR
		       "%s: request %p for device [%s] already tagged %d",
		       __func__, rq,
		       rq->rq_disk ? rq->rq_disk->disk_name : "?", rq->tag);
		BUG();
	}

	/*
	 * Protect against shared tag maps, as we may not have exclusive
	 * access to the tag map.
	 *
	 * We reserve a few tags just for sync IO, since we don't want
	 * to starve sync IO on behalf of flooding async IO.
	 */
	max_depth = bqt->max_depth;
	if (!rq_is_sync(rq) && max_depth > 1) {
		switch (max_depth) {
		case 2:
			max_depth = 1;
			break;
		case 3:
			max_depth = 2;
			break;
		default:
			max_depth -= 2;
		}
		if (q->in_flight[BLK_RW_ASYNC] > max_depth)
			return 1;
	}

	do {
		if (bqt->alloc_policy == BLK_TAG_ALLOC_FIFO) {
			tag = find_first_zero_bit(bqt->tag_map, max_depth);
			if (tag >= max_depth)
				return 1;
		} else {
			int start = bqt->next_tag;
			int size = min_t(int, bqt->max_depth, max_depth + start);
			tag = find_next_zero_bit(bqt->tag_map, size, start);
			if (tag >= size && start + size > bqt->max_depth) {
				size = start + size - bqt->max_depth;
				tag = find_first_zero_bit(bqt->tag_map, size);
			}
			if (tag >= size)
				return 1;
		}

	} while (test_and_set_bit_lock(tag, bqt->tag_map));
	/*
	 * We need lock ordering semantics given by test_and_set_bit_lock.
	 * See blk_queue_end_tag for details.
	 */

	bqt->next_tag = (tag + 1) % bqt->max_depth;
	rq->cmd_flags |= REQ_QUEUED;
	rq->tag = tag;
	bqt->tag_index[tag] = rq;
	blk_start_request(rq);
	list_add(&rq->queuelist, &q->tag_busy_list);
	return 0;
}


static void scsi_request_fn(struct request_queue *q)



struct request_queue *scsi_alloc_queue(struct scsi_device *sdev)
{
	struct request_queue *q;

	q = __scsi_alloc_queue(sdev->host, scsi_request_fn);
	if (!q)
		return NULL;

	blk_queue_prep_rq(q, scsi_prep_fn);
	blk_queue_unprep_rq(q, scsi_unprep_fn);
	blk_queue_softirq_done(q, scsi_softirq_done);
	blk_queue_rq_timed_out(q, scsi_times_out);
	blk_queue_lld_busy(q, scsi_lld_busy);
	return q;
}
