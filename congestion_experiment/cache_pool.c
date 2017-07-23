static struct kmem_cache *cfq_pool;

static void cfq_put_queue(struct cfq_queue *cfqq){
	kmem_cache_free(cfq_pool, cfqq);
}

static struct cfq_queue * cfq_get_queue(struct cfq_data *cfqd, bool is_sync, struct cfq_io_cq *cic,
	      struct bio *bio){
	rcu_read_lock();
    	cfqq = kmem_cache_alloc_node(cfq_pool, GFP_NOWAIT | __GFP_ZERO,
    				     cfqd->queue->node);
    	if (!cfqq) {
    		cfqq = &cfqd->oom_cfqq;
    		goto out;
    	}
	rcu_read_unlock();
	return cfqq;
}


static int cfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio,
		gfp_t gfp_mask){
	spin_lock_irq(q->queue_lock);
	cfqq = cfq_get_queue(cfqd, is_sync, cic, bio);
	rq->elv.priv[0] = cfqq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static int __init cfq_init(void){
	cfq_pool = KMEM_CACHE(cfq_queue, 0);
	return ret;
}
