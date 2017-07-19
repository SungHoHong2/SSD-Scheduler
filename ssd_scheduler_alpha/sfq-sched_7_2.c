/*
 * elevator noop
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


typedef struct sfq_request {

    // SFQ Algorithm
    int start_tag;
    int finish_tag;

    pid_t pid;
    struct request *rq;

    int valid;

} sfq_request;


typedef struct sfq_data {
  // SFQ Algorithm
  int virtual_time;

  // Heap Sort
  int size ;
  sfq_request **requests;

  // FRISK
	struct list_head queue;
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

int get_highest_finish_tag(sfq_data *hp, int i) {
    int l, r;
    if(LCHILD(i) >= hp->size) {
        return hp->requests[i]->start_tag + (REQUEST_LENGTH / REQUEST_WEIGHT);
    }

    l = get_highest_finish_tag(hp, LCHILD(i)) ;
    r = get_highest_finish_tag(hp, RCHILD(i)) ;

    if(l >= r) {
        return l + (REQUEST_LENGTH / REQUEST_WEIGHT);
    } else {
        return r + (REQUEST_LENGTH / REQUEST_WEIGHT);
    }
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

  // initialize size of the heap array
  sfqd->size = 0;
	eq->elevator_data = sfqd;


	INIT_LIST_HEAD(&sfqd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("INIT_SFQ 113\n");
	return 0;
}


static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){

     struct sfq_data *sfqd = q->elevator->elevator_data;
     struct sfq_request *sfqr;
     int latest_finish_tag;

     sfqr = (struct sfq_request*)kmalloc(sizeof(struct sfq_request), gfp_mask);
     sfqr->pid = current->pid;
     sfqr->valid = 1;

     sfqr->start_tag = sfqd->virtual_time;
     // latest_finish_tag = sfqr->start_tag + ( REQUEST_LENGTH / REQUEST_WEIGHT );


     // update the virtual_time with the smallest outstanding request
     if(sfqd && sfqd->size>0 && sfqd->requests[0] && sfqd->requests[0]->start_tag >=0){
       // sfqd->virtual_time = sfqd->requests[0]->start_tag;
        // printk("before size : %d\n", sfqd->size);
        // printk("latest finish_tag: %d\n", sfqd->requests[(sfqd->size)-1]->finish_tag);
        latest_finish_tag = sfqd->requests[(sfqd->size)-1]->finish_tag;
        if(sfqd->virtual_time < latest_finish_tag){
              sfqd->virtual_time = latest_finish_tag;
        }

        sfqr->start_tag = sfqd->virtual_time;
       // error latest_finish_tag = get_highest_finish_tag(sfqd, 0);
     }


    sfqr->finish_tag = sfqr->start_tag + ( REQUEST_LENGTH / REQUEST_WEIGHT );

     rq->elv.priv[0] = sfqr;

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

   if(sfqd->size) {
       sfqd->requests = (sfq_request **)krealloc(sfqd->requests, (sfqd->size + 1) * sizeof(sfq_request *), GFP_KERNEL) ;
   } else {
       sfqd->requests = (sfq_request **)kmalloc(sizeof(sfq_request *), GFP_KERNEL) ;
   }

   i = (sfqd->size)++ ;

   while(i && sfqr->start_tag < sfqd->requests[PARENT(i)]->start_tag) {
       sfqd->requests[i] = sfqd->requests[PARENT(i)] ;
       i = PARENT(i) ;
   }

   sfqr->rq = rq;
   sfqd->requests[i] = sfqr ;

  }

  // printk("1 ADD_REQUEST[ sfqr_pid: %d, start_tag: %d, valid: %d ] \n", sfqd->requests[0]->pid, sfqd->requests[0]->start_tag, sfqd->requests[0]->valid);
  // printk("2 ADD_REQUEST[ sfqr_pid: %d, start_tag: %d, valid: %d ] \n", sfqr->pid, sfqr->start_tag, sfqr->valid);
  // printk("3 ADD_REQUEST[ virtual_time : %d,  size: %d  ] \n", sfqd->virtual_time, sfqd->size);
	list_add_tail(&rq->queuelist, &sfqd->queue);
}



static int sfq_dispatch(struct request_queue *q, int force){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_request *sfqr;
  struct request *rq;


  if(sfqd && sfqd->size>0){
    // I suspect here
    // printk("0 SET DISPATCH: pid: %d, start_tag: %d\n",sfqd->requests[0]->pid, sfqd->requests[0]->start_tag);

    // lest try to check what the first request actually offers???
    printk("0 SET DISPATCH: pid: %d, size: %d\n",sfqd->requests[0]->pid, sfqd->size);
    sfqr = sfqd->requests[0];
    rq = sfqr->rq;

    printk("0 RQ: size: %ld\n", rq->start_time);

    if(sfqd->size>1){
        sfqd->requests[0] = sfqd->requests[--(sfqd->size)] ;
        printk("1 SET DISPATCH: pid: %d, size: %d\n",sfqd->requests[0]->pid, sfqd->size);

        sfqd->requests = (sfq_request **)krealloc(sfqd->requests, sfqd->size * sizeof(sfq_request *), GFP_KERNEL) ;
        printk("2 SET DISPATCH: pid: %d, size: %d\n",sfqd->requests[0]->pid, sfqd->size);
        heapify(sfqd, 0) ;
        printk("3 SET DISPATCH: pid: %d, size: %d\n",sfqd->requests[0]->pid, sfqd->size);

        list_del_init(&rq->queuelist);
        elv_dispatch_sort(q, rq);
        return 1;
    }else {
          return 0;
    }

  }


	rq = list_first_entry_or_null(&sfqd->queue, struct request, queuelist);

	if (rq) {
    // check the structure of request and find out whether there are valid values
    // printk("ARRAY: %ld\n", sfqr->rq->start_time);
    // printk("QUEUE: %ld\n", rq->start_time);

		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1; // without this the dispatch is stucked
	}

	return 0; // return results in infinite lock
            // gets an error if there are no dispatch
}



static void sfq_exit_queue(struct elevator_queue *e){
	struct sfq_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}


static struct elevator_type elevator_sfq = {
	.ops = {
    .elevator_exit_fn		= sfq_exit_queue,
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
