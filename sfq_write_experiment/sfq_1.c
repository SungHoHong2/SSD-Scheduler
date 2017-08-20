#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>





struct sfq_data {
	struct list_head queue;
};


static int sfq_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sfq_data *nd;
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

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("INIT_SFQ WRITE ERROR SEARCH\n");

	return 0;
}

static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
  struct sfq_data *sfqd = q->elevator->elevator_data;

  if(!sfqd || !rq || !bio) return 0;

  printk("SET_REQUEST\n");

  return 0;
}


static void sfq_add_request(struct request_queue *q, struct request *rq)
{
	struct sfq_data *nd = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &nd->queue);

  printk("ADD_REQUEST\n");
}



static int sfq_dispatch(struct request_queue *q, int force)
{
	struct sfq_data *nd = q->elevator->elevator_data;
	struct request *rq;

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	if (rq) {
    printk("DISPATCH_REQUEST\n");
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}


static void sfq_completed(struct request_queue *q, struct request *rq){
}

static void sfq_put_request(struct request *rq){

}


static void sfq_exit_queue(struct elevator_queue *e)
{
	struct sfq_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
    .elevator_put_req_fn =		sfq_put_request,
    .elevator_completed_req_fn  = sfq_completed,
		.elevator_dispatch_fn		= sfq_dispatch,
		.elevator_add_req_fn		= sfq_add_request,
    .elevator_set_req_fn = sfq_set_request,
		.elevator_init_fn		= sfq_init_queue,
		.elevator_exit_fn		= sfq_exit_queue,
	},
	.elevator_name = "sfq",
	.elevator_owner = THIS_MODULE,
};

static int __init sfq_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit sfq_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(sfq_init);
module_exit(sfq_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
