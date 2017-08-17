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
#include <linux/blk-cgroup.h>


//  74,1176128 write /home/sungho/Documents/SSD-Scheduler/test_results/2017_08_02_workstation/test

struct noop_data {
	struct list_head queue;
  long start_track_time;
  struct timeval start_time;
  struct timeval current_time;

};

static int track_array[61][1];

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
	sector_t sector;


	if(!nd) return 0;

	if(!rq) return 0;

	if(!bio) return 0;


	sector = bio_end_sector(bio);
	if(!sector) return 0;



	if (sector < blk_rq_pos(rq)){
		printk("bi_iter.bi_sector: [[[%ld]]] \n", sector);

		sector = (bio)->bi_iter.bi_sector;
		printk("bi_iter.bi_sector: [[[%ld]]] \n", sector);

		// sector = bio_offset(bio);
		// printk("chara bio_offset: %ld \n", sector);

	}

	// else if (sector > blk_rq_pos(rq))
		// printk("chara go right \n");
	// else
		// printk("chara done nothing \n");

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
	int i;

	printk("RESULTS:\n");
	for(i=0; i<61; i++){
		printk("runtime %d\t\t %d\n",i+1, track_array[i][0]);
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
