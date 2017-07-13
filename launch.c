/*
 * elevator sfq
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/delay.h>

// assume all requests have fixed size
#define REQUEST_LENGTH 100

// assume all requests are read
#define REQUEST_WEIGHT 1

// Heap Sort - Indexing
#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2

// Total Depth [MIN:1 - MAX:64]
// Estimated Efficient number [36(WRITE) - 40(READ)]
#define REQUEST_DEPTH 1

typedef struct sfq_request {

    // SFQ Algorithm
    int start_tag;
    int finish_tag;
    pid_t pid;

    // Assign requests
    struct request *rq;

    struct list_head stack_list;
    struct sfq_data *sfqd;

} sfq_request;



typedef struct sfq_data {

  // SFQ Algorithm
  int virtual_time;

  // Heap Sort
  int size;
  sfq_request **requests;

  // number of outstanding I/O requests
  int depth;

  // tracking  outstanding request
	sfq_request *curr_sfqr;

  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *queue;


  struct list_head sfqr_queue;


} sfq_data;



/*
 * Heap Sorting Common Function
 */

void heap_swap(sfq_request *n1, sfq_request *n2) {
    sfq_request temp = *n1 ;
    *n1 = *n2 ;
    *n2 = temp ;
}

// find the smallest node at index i by comparing parent, left and right child
void heapify(sfq_data *hp, int i) {
    int smallest = (LCHILD(i) < hp->size && hp->requests[LCHILD(i)]->start_tag < hp->requests[i]->start_tag) ? LCHILD(i) : i ;
    if(RCHILD(i) < hp->size && hp->requests[RCHILD(i)]->start_tag < hp->requests[smallest]->start_tag) {
        smallest = RCHILD(i) ;
    }
    if(smallest != i) {
        heap_swap(hp->requests[i], hp->requests[smallest]) ;
        heapify(hp, smallest) ;
    }
}


/*
 * Elevator common functions
 */

 static void sfq_kick_queue(struct work_struct *work){
 	struct sfq_data *sfqd =
 		container_of(work, struct sfq_data, unplug_work);
 	struct request_queue *q = sfqd->queue;

 	spin_lock_irq(q->queue_lock);
 	__blk_run_queue(sfqd->queue);
 	spin_unlock_irq(q->queue_lock);
 }



/*
 * sfq scheduler
 */

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



  // initialize virtual time
  sfqd->virtual_time = 0;
  sfqd->requests = (sfq_request **)kmalloc(sizeof(sfq_request *), GFP_KERNEL);

  // initialize size of the heap array
  sfqd->size = 0;

  // initialize the depth to 0
  sfqd->depth = 0;

  // insurance for invoking dispatch during pending requests
  sfqd->queue = q;
  INIT_WORK(&sfqd->unplug_work, sfq_kick_queue);
  INIT_LIST_HEAD(&sfqd->sfqr_queue);
  INIT_LIST_HEAD(&sfqd->stack_queue);


	eq->elevator_data = sfqd;
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("INIT_SFQ NOOP TEST \n");
  //613392,735228 write /media/sf_SSD-Scheduler/test_results/2017_07_11_depth_results/sfq_test_02.txt

	return 0;
}


static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){

     struct sfq_data *sfqd = q->elevator->elevator_data;
     struct sfq_request *sfqr;
     int latest_finish_tag;

     sfqr = (struct sfq_request*)kmalloc(sizeof(struct sfq_request), gfp_mask);
     sfqr->pid = current->pid;
     sfqr->start_tag = sfqd->virtual_time;
     sfqr->sfqd = sfqd;


     

    //
    //
    //  // update the virtual_time with the smallest outstanding request
    //  if(sfqd && sfqd->size>0 && sfqd->requests[0] && sfqd->requests[0]->start_tag >=0){
    //     latest_finish_tag = sfqd->requests[(sfqd->size)-1]->finish_tag;
    //     if(sfqd->virtual_time < latest_finish_tag){
    //           sfqd->virtual_time = latest_finish_tag;
    //     }
    //     sfqr->start_tag = sfqd->virtual_time;
    //  }
    //
    //
    // sfqr->finish_tag = sfqr->start_tag + ( REQUEST_LENGTH / REQUEST_WEIGHT );
    // rq->elv.priv[0] = sfqr;

    // if(sfqd && sfqr)
    // printk("SET_REQUEST[ virtual_time : %d,  pid: %d, start_tag : %d] \n", sfqd->virtual_time, sfqr->pid, sfqr->start_tag);

     return 0;
}


static void sfq_add_request(struct request_queue *q, struct request *rq){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_request *sfqr = rq->elv.priv[0];
  int i;

   // assign the request -> sfq_request struct
   if(sfqr->start_tag >= 0){

  //  if(sfqd->size) {
  //      sfqd->requests = (sfq_request **)krealloc(sfqd->requests, (sfqd->size + 1) * sizeof(sfq_request *), GFP_KERNEL) ;
  //  }
   //
  //  i = (sfqd->size)++ ;
   //
  //  while(i && sfqr->start_tag < sfqd->requests[PARENT(i)]->start_tag) {
  //      sfqd->requests[i] = sfqd->requests[PARENT(i)] ;
  //      i = PARENT(i) ;
  //  }


  list_add_tail(&rq->queuelist, &sfqd->stack_queue);



  //  sfqd->requests[i] = sfqr ;
  }
  printk("ADD_REQUEST[ size: %d  ] \n",sfqd->size);
  list_add_tail(&rq->queuelist, &sfqd->sfqr_queue);

}


static int sfq_dispatch(struct request_queue *q, int force){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_request *sfqr;
  struct request *rq;

  // // check the number of depth
  // if(sfqd && sfqd->size>0){
  //
  //   // sfqr = sfqd->requests[0];
  //   // rq = sfqr->rq;
  //   sfqd->requests[0] = sfqd->requests[--(sfqd->size)];
  //
  //   if(sfqd->size>1){
  //       sfqd->requests = (sfq_request **)krealloc(sfqd->requests, sfqd->size * sizeof(sfq_request *), GFP_KERNEL) ;
  //       heapify(sfqd, 0);
  //   }
  // }


  rq = list_first_entry_or_null(&sfqd->sfqr_queue, struct request, queuelist);
  if (rq) {
    // printk("DISPATCH: DEPTH: %d\n", sfqd->depth);
    list_del_init(&rq->queuelist);
    elv_dispatch_sort(q, rq);
    sfqd->curr_sfqr = sfqr;
    sfqd->depth++;
    return 1;
  }

  return 0;
}


// static void sfq_completed(struct request_queue *q, struct request *rq){
// 	 struct sfq_data *sfqd = q->elevator->elevator_data;
// 	 // printk("COMPLETE: PID: %d  DEPTH: %d\n",sfqd->curr_sfqr->pid, sfqd->depth);
//    if(sfqd->size>0){
//       // invoke the dispatch again
//       kblockd_schedule_work(&sfqd->unplug_work);
//    }
// }




static void sfq_exit_queue(struct elevator_queue *e){
	struct sfq_data *nd = e->elevator_data;
	kfree(nd);
}


static struct elevator_type elevator_sfq = {
	.ops = {
    .elevator_exit_fn		= sfq_exit_queue,
  //  .elevator_completed_req_fn  = sfq_completed,
		.elevator_dispatch_fn		= sfq_dispatch,
		.elevator_add_req_fn		= sfq_add_request,
    .elevator_set_req_fn = sfq_set_request,
		.elevator_init_fn		= sfq_init_queue,
	},
	.elevator_name = "sfq",
	.elevator_owner = THIS_MODULE,
};

static int __init sfq_init(void){
	return elv_register(&elevator_sfq);
}

static void __exit sfq_exit(void){
	elv_unregister(&elevator_sfq);
}

module_init(sfq_init);
module_exit(sfq_exit);

MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SFQ IO scheduler");
