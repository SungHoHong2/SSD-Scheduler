/*
 * elevator noop
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#define REQUEST_DEPTH 1

static struct kmem_cache *noop_pool;

struct noop_request{
	struct list_head queuelist;
	struct request *rq;
};

struct noop_data {
	struct list_head queue;
  struct noop_request *out_req;
	int depth;
  int nr_size;

  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *rq_queue;

};


// Elevator common functions
 static void noop_kick_queue(struct work_struct *work){
 	struct noop_data *nd =
 		container_of(work, struct noop_data, unplug_work);
 	struct request_queue *q = nd->rq_queue;

 	spin_lock_irq(q->queue_lock);
 	__blk_run_queue(nd->rq_queue);
 	spin_unlock_irq(q->queue_lock);
 }

 static enum hrtimer_restart noop_idle_slice_timer(struct hrtimer *timer){
   unsigned long flags;
   struct noop_data *nd = container_of(timer, struct noop_data, idle_slice_timer);
   spin_lock_irqsave(nd->rq_queue->queue_lock, flags);
   if (!list_empty(&nd->queue)) kblockd_schedule_work(&nd->unplug_work);
   spin_unlock_irqrestore(nd->rq_queue->queue_lock, flags);
	 return HRTIMER_NORESTART;
 }


static int noop_init_queue(struct request_queue *q, struct elevator_type *e){
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
  nd->nr_size = 0;
  nd->rq_queue = q;
  INIT_WORK(&nd->unplug_work, noop_kick_queue);
  noop_pool = KMEM_CACHE(noop_request, 0);

  hrtimer_init(&nd->idle_slice_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  nd->idle_slice_timer.function = noop_idle_slice_timer;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);


  printk("NOOP EXPERIMEN BETA DEPTH: %d\n",REQUEST_DEPTH);
	return 0;
}

static int noop_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
  struct noop_data *nd = q->elevator->elevator_data;
  struct noop_request *nr;
  rcu_read_lock();
  nr = kmem_cache_alloc_node(noop_pool, GFP_NOWAIT | __GFP_ZERO, nd->rq_queue->node);
  rcu_read_unlock();

  rq->elv.priv[0] = nr;
  return 0;
}


static void noop_add_request(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
  struct noop_request *nr = rq->elv.priv[0];
  nr->rq = rq;
  nd->nr_size++;
  list_add_tail(&nr->queuelist, &nd->queue);
}

static int noop_dispatch(struct request_queue *q, int force){
	struct noop_data *nd = q->elevator->elevator_data;
  // struct noop_request *nr;

  if(nd && nd->depth>REQUEST_DEPTH) return 0;

  hrtimer_try_to_cancel(&nd->idle_slice_timer);
  nd->out_req = list_first_entry_or_null(&nd->queue, struct noop_request, queuelist);

	if (nd->out_req) {
    // printk("DISPATCH: depth: %d\n",nd->depth);
		list_del_init(&nd->out_req->queuelist);
		elv_dispatch_sort(q, nd->out_req->rq);
    nd->depth++;
		return 1;
	}
	return 0;
}

static void noop_completed(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
  // printk("COMPLETE: depth: %d\n",nd->depth);
  u64 sl = 0;
  nd->depth--;
  if((--nd->nr_size)>0){
      hrtimer_start(&nd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
      kblockd_schedule_work(&nd->unplug_work);
  }
}

static void noop_put_request(struct request *rq){
  struct noop_request *nr = rq->elv.priv[0];
  if(nr) kmem_cache_free(noop_pool, nr);
}

static void noop_exit_queue(struct elevator_queue *e){
	struct noop_data *nd = e->elevator_data;
	BUG_ON(!list_empty(&nd->queue));
  hrtimer_cancel(&nd->idle_slice_timer);
  cancel_work_sync(&nd->unplug_work);
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
    .elevator_put_req_fn =		noop_put_request,
		.elevator_completed_req_fn  = noop_completed,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
    .elevator_set_req_fn = noop_set_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "sfq",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void){
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void){
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
