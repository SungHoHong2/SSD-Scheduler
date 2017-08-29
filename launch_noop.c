/*
 * elevator noop
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>


struct noop_data {
	struct list_head queue;

  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *rq_queue;

  int chara_locker;
  int sfqr_size;

};



// Elevator common functions
 static void sfq_kick_queue(struct work_struct *work){
 	struct noop_data *sfqd =
 		container_of(work, struct noop_data, unplug_work);
 	struct request_queue *q = sfqd->rq_queue;

 	spin_lock_irq(q->queue_lock);
 	__blk_run_queue(sfqd->rq_queue);
 	spin_unlock_irq(q->queue_lock);
 }

 // static enum hrtimer_restart sfq_idle_slice_timer(struct hrtimer *timer){
 // 	unsigned long flags;
 // 	struct noop_data *sfqd = container_of(timer, struct noop_data, idle_slice_timer);
 // 	spin_lock_irqsave(sfqd->rq_queue->queue_lock, flags);
 // 	if (sfqd->sfqr_size) kblockd_schedule_work(&sfqd->unplug_work);
 // 	spin_unlock_irqrestore(sfqd->rq_queue->queue_lock, flags);
 //  return HRTIMER_NORESTART;
 // }






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

  nd->chara_locker = 0;
  nd->sfqr_size = 0;
	nd->rq_queue = q;
 	INIT_LIST_HEAD(&nd->queue);
  INIT_WORK(&nd->unplug_work, sfq_kick_queue);


 	spin_lock_irq(q->queue_lock);
 	q->elevator = eq;
 	spin_unlock_irq(q->queue_lock);


   printk("noop_experiment\n");

 	return 0;
 }


 static void noop_merged_requests(struct request_queue *q, struct request *rq,
 				 struct request *next)
 {
 	list_del_init(&next->queuelist);
 }


 static void noop_add_request(struct request_queue *q, struct request *rq)
 {
 	struct noop_data *nd = q->elevator->elevator_data;

  nd->sfqr_size++;
 	list_add_tail(&rq->queuelist, &nd->queue);
	__blk_run_queue(nd->rq_queue);

 }


static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

	if(!nd) return 0;

	if(nd->sfqr_size){ //&& sfqd->chara_locker){
			kblockd_schedule_work(&nd->unplug_work);
	}

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}



static void noop_completed(struct request_queue *q, struct request *rq){
  struct noop_data *sfqd = q->elevator->elevator_data;

	if((--sfqd->sfqr_size)>0){
     // invoke the dispatch again
     // hrtimer_start(&sfqd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
     if(sfqd->sfqr_size) kblockd_schedule_work(&sfqd->unplug_work);
  }

}



static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_merge_req_fn		= noop_merged_requests,
    .elevator_completed_req_fn  = noop_completed,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "sfq",
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
