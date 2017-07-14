/*
 * elevator noop
   when the noop runs with 64jobs with 1 depth it freezes
	 when the noop runs with 32jobs with 1 depth it works
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#define REQUEST_DEPTH 1

struct noop_data {
	struct request_queue *queue;
	int depth;

	// invoking dispatch in complete function
	struct hrtimer idle_slice_timer;
	struct work_struct unplug_work;
	struct list_head noop_queue;

};


static void noop_kick_queue(struct work_struct *work){
 struct noop_data *nd =
	 container_of(work, struct noop_data, unplug_work);
 struct request_queue *q = nd->queue;

 spin_lock_irq(q->queue_lock);
 __blk_run_queue(nd->queue);
 spin_unlock_irq(q->queue_lock);
}


static void noop_completed(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
  nd->depth--;

	if(nd->depth>0){
 	 kblockd_schedule_work(&nd->unplug_work);
	 }
}

static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

  if(nd->depth<=REQUEST_DEPTH){

  	rq = list_first_entry_or_null(&nd->noop_queue, struct request, queuelist);
  	if (rq) {
      nd->depth++;
  		list_del_init(&rq->queuelist);
  		elv_dispatch_sort(q, rq);
  		return 1;
  	}
  }
	return 0;
}

static void noop_add_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->noop_queue);
}


static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
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
	INIT_LIST_HEAD(&nd->noop_queue);

	nd->queue = q;
  INIT_WORK(&nd->unplug_work, noop_kick_queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);


  printk("NOOP EXPERIMENT %d\n",REQUEST_DEPTH);
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->noop_queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_completed_req_fn  = noop_completed,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "noop_test",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
