#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

// assume all requests have fixed size
#define REQUEST_LENGTH 100
// assume all requests are read
#define REQUEST_WEIGHT 1

typedef struct sfq_request {
   // SFQ Algorithm
   int start_tag;
   int finish_tag;
   // Assign requests
   struct request *rq;
   // link to head of sfq_queue
   struct list_head queuelist;
   // link to global data
   struct sfq_data *sfqd;
} sfq_request;


typedef struct sfq_queue {
   // link to head of sfq_data
   struct list_head queuelist;
   // head of sfq_request
   struct list_head queue;
   // unique id
   pid_t pid;
} sfq_queue;


typedef struct sfq_data {
  // SFQ Algorithm
  int virtual_time;
  // Heap Sort
  int size;
  sfq_request **requests;
  // tracking previous sfq_queue
  sfq_request *prev_sfqr;
  // head of sfq_queue
  struct list_head queue;
  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *rq_queue;
  //NOOP
  struct list_head heap_queue;
} sfq_data;


static int sfq_init_queue(struct request_queue *q, struct elevator_type *e){
	struct sfq_data *sfqd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	sfqd = kmalloc_node(sizeof(*sfqd), GFP_KERNEL, q->node);
	if (!sfqd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = sfqd;
  sfqd->virtual_time = 0;
  sfqd->prev_sfqr = NULL;
  INIT_LIST_HEAD(&sfqd->queue);

  //NOOP
	INIT_LIST_HEAD(&sfqd->heap_queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("INIT_SFQ BETA\n");
  //47,53 write /media/sf_SSD-Scheduler/test_results/2017_07_19_beta/sfq_test_01.txt
	return 0;
}


static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
  struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_queue *sfqq;
  struct sfq_request *sfqr;
  struct list_head *head;

  // check for existing pids
  list_for_each(head,&(sfqd->queue)){
     sfqq = list_entry(head,struct sfq_queue, queuelist);
     if (sfqq && sfqq->pid == current->pid){
        rq->elv.priv[0] = sfqq;
        goto skip_sfq_queue;
     }
   }

  // allocate sfq_queue
  sfqq = (struct sfq_queue*)kmalloc(sizeof(struct sfq_queue), gfp_mask);
  sfqq->pid = current->pid;
  INIT_LIST_HEAD(&sfqq->queue);
  rq->elv.priv[0] = sfqq;
  list_add_tail(&sfqq->queuelist, &sfqd->queue);
  printk("SET_REQUEST PID: %d virtual_time: %d\n", sfqq->pid, sfqd->virtual_time);

  skip_sfq_queue:
  // allocate sfq_request
  sfqr = (struct sfq_request*)kmalloc(sizeof(struct sfq_request), gfp_mask);
  // start_tag = prev_arrival_time(finish_tag) vs virutal_time(start_tag)
  if(sfqd->prev_sfqr && (sfqd->virtual_time < sfqd->prev_sfqr->finish_tag))
      sfqr->start_tag = sfqd->prev_sfqr->finish_tag;
  else sfqr->start_tag = sfqd->virtual_time;
  // finish_tag
  sfqr->finish_tag = sfqr->start_tag + (REQUEST_LENGTH / REQUEST_WEIGHT);
  //update virtual_time
  sfqd->virtual_time = sfqr->start_tag;
  //previous request
  sfqd->prev_sfqr = sfqr;
  rq->elv.priv[1] = sfqr;
  return 0;
}


static void sfq_add_request(struct request_queue *q, struct request *rq){
	struct sfq_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->heap_queue);
}



static int sfq_dispatch(struct request_queue *q, int force){
	struct sfq_data *nd = q->elevator->elevator_data;
	struct request *rq;

	rq = list_first_entry_or_null(&nd->heap_queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}


static struct request *
sfq_former_request(struct request_queue *q, struct request *rq){
	struct sfq_data *nd = q->elevator->elevator_data;

  // check whether the request is the immmediate dispatch
  // in that case we can check whether the request is within the dispatch_array
	if (rq->queuelist.prev == &nd->heap_queue)
		return NULL;
	return list_prev_entry(rq, queuelist);
}

static struct request *
sfq_latter_request(struct request_queue *q, struct request *rq){
	struct sfq_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->heap_queue)
		return NULL;
	return list_next_entry(rq, queuelist);
}


static void sfq_completed(struct request_queue *q, struct request *rq){
  //  struct sfq_data *sfqd = q->elevator->elevator_data;
}

static void sfq_put_request(struct request *rq){
    // if there are no more requests remove it from the queue_head of sfq_data
}


static void sfq_exit_queue(struct elevator_queue *e){
	struct sfq_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->heap_queue));
	kfree(nd);
}


static struct elevator_type elevator_noop = {
	.ops = {
    .elevator_exit_fn		= sfq_exit_queue,
    .elevator_put_req_fn =		sfq_put_request,
    .elevator_completed_req_fn  = sfq_completed,
    .elevator_former_req_fn		= sfq_former_request,
    .elevator_latter_req_fn		= sfq_latter_request,
    .elevator_dispatch_fn		= sfq_dispatch,
    .elevator_add_req_fn		= sfq_add_request,
    .elevator_set_req_fn = sfq_set_request,
    .elevator_init_fn		= sfq_init_queue,
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


MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SFQ IO scheduler");
