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
