/*
 * elevator noop
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/time.h>

//  74,1176128 write /home/sungho/Documents/SSD-Scheduler/test_results/2017_08_02_workstation/test

struct noop_data {
	struct list_head queue;
  long start_track_time;
  struct timeval start_time;
  struct timeval current_time;

};

int track_array[61][1];

static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static void noop_add_request(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->queue);
}


static int noop_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
  struct noop_data *nd = q->elevator->elevator_data;
  long track_time = 0;
	int temp = 0;

  if(nd->start_track_time==0){
    do_gettimeofday(&nd->start_time); //it is known as jiffies
    nd->start_track_time = (u32)(nd->start_time.tv_sec);
    // printk("0\t\tfirst request\n");
		temp = track_array[0][0];
		temp++;
		track_array[0][0] = temp;

  }else{
    do_gettimeofday(&nd->current_time);
    track_time = (u32)(nd->current_time.tv_sec);
    // printk("%ld\t\treceive request\n", track_time-(nd->start_track_time));
		temp = track_array[track_time][0];
		temp++;
		track_array[track_time][0] = temp;

  }
  return 0;
}


static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct noop_data *nd;
	struct elevator_queue *eq;
	int i=0;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;


	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
  nd->start_track_time = 0;

	for(i=0; i<61; i++){
		track_array[i][0] = 0;
	}

	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("noop_experiment initialized\n");
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;
	int i=0;


	printk("RESULTS:\n");
	for(i=0; i<61; i++){
		printk("job %d\t\t %d",i+1, track_array[i][0]);
	}


	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
    .elevator_set_req_fn = noop_set_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "noop_experiment",
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
