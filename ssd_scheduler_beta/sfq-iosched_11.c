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
// Heap Sort - Indexing
#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2
// depth
#define REQUEST_DEPTH 1

static struct kmem_cache *sfq_pool;

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
  // allowed number of outstanding requests
  int depth;
  // Heap Sort
  int size;
  sfq_request **requests;
  // tracking previous sfq_queue
  sfq_request *prev_sfqr;
  // outstanding sfq_queue
  sfq_queue *os_sfqq;
  // head of sfq_queue
  struct list_head queue;
  // total number of sfq_queue
  int sfqq_size;
  int sfqq_total_seek;
  // total number of sfq_request
  int sfqr_size;
  // total number of heap_size
  int heap_size;
  int heap_limit_size;
  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *rq_queue;

} sfq_data;


// Elevator common functions
 static void sfq_kick_queue(struct work_struct *work){
 	struct sfq_data *sfqd =
 		container_of(work, struct sfq_data, unplug_work);
 	struct request_queue *q = sfqd->rq_queue;

 	spin_lock_irq(q->queue_lock);
 	__blk_run_queue(sfqd->rq_queue);
 	spin_unlock_irq(q->queue_lock);
 }
 static enum hrtimer_restart sfq_idle_slice_timer(struct hrtimer *timer){
   unsigned long flags;
   struct sfq_data *sfqd = container_of(timer, struct sfq_data, idle_slice_timer);
   spin_lock_irqsave(sfqd->rq_queue->queue_lock, flags);
   if (sfqd->sfqr_size) kblockd_schedule_work(&sfqd->unplug_work);
   spin_unlock_irqrestore(sfqd->rq_queue->queue_lock, flags);
	 return HRTIMER_NORESTART;
 }


// Heap Sorting Common Function
 void heap_swap(sfq_request *n1, sfq_request *n2) {
     sfq_request temp = *n1 ;
     *n1 = *n2 ;
     *n2 = temp ;
 }
 void heapify(sfq_data *hp, int i) {
     int smallest = (LCHILD(i) < hp->heap_size && hp->requests[LCHILD(i)]->start_tag < hp->requests[i]->start_tag) ? LCHILD(i) : i ;
     if(RCHILD(i) < hp->heap_size && hp->requests[RCHILD(i)]->start_tag < hp->requests[smallest]->start_tag) {
         smallest = RCHILD(i) ;
     }
     if(smallest != i) {
         heap_swap(hp->requests[i], hp->requests[smallest]) ;
         heapify(hp, smallest) ;
     }
 }


static int sfq_init_queue(struct request_queue *q, struct elevator_type *e){
	struct sfq_data *sfqd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq) return -ENOMEM;
	sfqd = kmalloc_node(sizeof(*sfqd), GFP_KERNEL, q->node);
	if (!sfqd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = sfqd;
  sfqd->virtual_time = 0;
  sfqd->depth = 0;
  sfqd->prev_sfqr = NULL;
  sfqd->os_sfqq = NULL;
  sfqd->sfqq_size = 0;
  sfqd->sfqq_total_seek = 0;
  sfqd->sfqr_size = 0;
  sfqd->heap_size = 0;
  sfqd->heap_limit_size = 0;
  sfq_pool = KMEM_CACHE(sfq_request, 0);
  INIT_LIST_HEAD(&sfqd->queue);
  // insurance for invoking dispatch during pending requests
  sfqd->rq_queue = q;
  INIT_WORK(&sfqd->unplug_work, sfq_kick_queue);
  hrtimer_init(&sfqd->idle_slice_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  sfqd->idle_slice_timer.function = sfq_idle_slice_timer;

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
  printk("INIT_SFQ TESTING_DISPATCH_DEPTH: %d ALPHA\n",REQUEST_DEPTH);
  //  1729998,2068302 write /media/sf_SSD-Scheduler/test_results/2017_07_19_beta/sfq_test_04.txt
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

  // create a heap_queue
  if(!sfqd->heap_limit_size){
    sfqd->requests = (sfq_request **)kmalloc(sizeof(sfq_request *), gfp_mask);
    sfqd->heap_limit_size++;
  } else {
    sfqd->requests = (sfq_request **)krealloc(sfqd->requests, (sfqd->heap_limit_size + 1) * sizeof(sfq_request *), GFP_KERNEL);
    sfqd->heap_limit_size++;
  }
  // printk("SET_REQUEST PID: %d heap_limit_size: %d\n", current->pid, sfqd->heap_limit_size);

  // allocate sfq_queue
  sfqq = (struct sfq_queue*)kmalloc(sizeof(struct sfq_queue), gfp_mask);
  sfqq->pid = current->pid;
  sfqd->sfqq_size++;
  INIT_LIST_HEAD(&sfqq->queue);
  rq->elv.priv[0] = sfqq;
  // add to sfq_data
  list_add_tail(&sfqq->queuelist, &sfqd->queue);
  // printk("\t\tSET_REQUEST PID: %d virtual_time: %d\n", sfqq->pid, sfqd->virtual_time);

  skip_sfq_queue:
  // allocate sfq_request
  rcu_read_lock();
  sfqr = kmem_cache_alloc_node(sfq_pool, GFP_NOWAIT | __GFP_ZERO, sfqd->rq_queue->node);
  rcu_read_unlock();
  rq->elv.priv[1] = sfqr;
  return 0;
}


static void sfq_add_request(struct request_queue *q, struct request *rq){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_queue *sfqq = rq->elv.priv[0];
  struct sfq_request *sfqr = rq->elv.priv[1];
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
  sfqr->rq = rq;
  // printk("ADD_REQUEST PID: %d\n", sfqq->pid);
  list_add_tail(&sfqr->queuelist, &sfqq->queue);
  sfqd->sfqr_size++;
}


static int sfq_dispatch(struct request_queue *q, int force){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  // struct list_head *head;
  struct sfq_queue *sfqq;
  struct sfq_request *sfqr;
  struct request *rq;
  int index;

  hrtimer_try_to_cancel(&sfqd->idle_slice_timer);

  // dispatched for the first time
  if(!(sfqd->os_sfqq)){
      sfqq = sfqd->os_sfqq = list_first_entry_or_null(&sfqd->queue, struct sfq_queue, queuelist);
      if(!(sfqq)) return 0;
      sfqd->sfqq_total_seek++;
      // printk("FIRST_DISPATCH PID: %d SEEK: %d  SIZE: %d\n", sfqd->os_sfqq->pid, sfqd->sfqq_total_seek, sfqd->sfqq_size);

  // currently dispatching
  } else {
  next_dispatch:
      sfqq = list_next_entry(sfqd->os_sfqq, queuelist);
      sfqd->os_sfqq = sfqq;
      if(sfqd->sfqq_total_seek == sfqd->sfqq_size){
        sfqd->sfqq_total_seek = 0;
        goto next_dispatch;
      }
      sfqd->sfqq_total_seek++;

      // check for left requests
      if(sfqd->sfqr_size && list_empty(&sfqd->os_sfqq->queue)){
          kblockd_schedule_work(&sfqd->unplug_work);
          // if empty proceed to the heap-queue dispatch
          goto dispatch_section;
      }
      // printk("NEXT_DISPATCH PID: %d SEEK: %d  SIZE: %d\n", sfqd->os_sfqq->pid, sfqd->sfqq_total_seek, sfqd->sfqq_size);
  }

  // limiting number of outstanding requests
  if(sfqd->depth > REQUEST_DEPTH){
      kblockd_schedule_work(&sfqd->unplug_work);
      return 0;
  }
  // transfer requests into heap-queue
  sfqr = list_first_entry_or_null(&sfqd->os_sfqq->queue, struct sfq_request, queuelist);
  if(sfqr){
    // printk("RQUEST to HEAP_QUEUE: %d PID: %d\n", sfqr->start_tag,sfqd->os_sfqq->pid);
    // remove request from sfq_queue
    if(sfqd->heap_size < sfqd->heap_limit_size-1){
        list_del_init(&sfqr->queuelist);
        index = (sfqd->heap_size)++;
        while(index && sfqr->start_tag < sfqd->requests[PARENT(index)]->start_tag) {
            sfqd->requests[index] = sfqd->requests[PARENT(index)] ;
            index = PARENT(index);
        }
        sfqd->requests[index] = sfqr;
    } else{
        kblockd_schedule_work(&sfqd->unplug_work);
    }
  }

  // perform the heap-sort
  dispatch_section:
  if(sfqd->heap_size>0){
      sfqr = sfqd->requests[0];
      rq = sfqr->rq;
      sfqd->requests[0] = sfqd->requests[--(sfqd->heap_size)];
      if(sfqd->heap_size>1) heapify(sfqd, 0);
      // printk("RQUEST to HEAP_QUEUE pid: %d  heap_size: %d\n", sfqd->os_sfqq->pid, sfqd->heap_size);
      // printk("RQUEST to HEAP_QUEUE depth: %d\n", sfqd->depth);
      elv_dispatch_sort(q, rq);
      sfqd->depth++;
      return 1;
  }
	return 0;
}

static void sfq_completed(struct request_queue *q, struct request *rq){
  struct sfq_data *sfqd = q->elevator->elevator_data;
  // printk("COMPLETE BEFORE PID: %d sfqr_size %d\n", sfqd->os_sfqq->pid, sfqd->sfqr_size);
  u64 sl = 0;
  sfqd->depth--;
  if((--sfqd->sfqr_size)>0){
     // invoke the dispatch again
     hrtimer_start(&sfqd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
     kblockd_schedule_work(&sfqd->unplug_work);
  }
}

static void sfq_put_request(struct request *rq){
  struct sfq_request *sfqr = rq->elv.priv[1];
  if(sfqr) kmem_cache_free(sfq_pool, sfqr);
}


static void sfq_exit_queue(struct elevator_queue *e){
	struct sfq_data *nd = e->elevator_data;
  hrtimer_cancel(&nd->idle_slice_timer);
  cancel_work_sync(&nd->unplug_work);
	kfree(nd);
}


static struct elevator_type elevator_noop = {
	.ops = {
    .elevator_exit_fn		= sfq_exit_queue,
    .elevator_put_req_fn =		sfq_put_request,
    .elevator_completed_req_fn  = sfq_completed,
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
