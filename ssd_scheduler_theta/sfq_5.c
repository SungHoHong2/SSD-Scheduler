#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>


#define REQUEST_LENGTH 100
// assume all requests are read
#define REQUEST_WEIGHT 1
// Heap Sort - Indexing
#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2
// depth
#define REQUEST_DEPTH 64

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
  // int sfqq_total_seek;
  // total number of sfq_request
  int sfqr_size;
  // total number of heap_size
  int heap_size;
  int heap_limit_size;
  // invoking dispatch in complete function
  struct hrtimer idle_slice_timer;
  struct work_struct unplug_work;
  struct request_queue *rq_queue;

  int chara_locker;
  int frisk_locker;

  struct sfq_queue *wrong_sfqq;

  struct list_head noop_queue;

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


//  2,19 write Documents/2017-01-06-Present-SungHoHong-Mac/Projects/SSD-Scheduler/test_results/2017_08_23_latency/test

static int sfq_init_queue(struct request_queue *q, struct elevator_type *e)
{
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
  sfqd->depth = 0;
  sfqd->prev_sfqr = NULL;
  sfqd->os_sfqq = NULL;
  sfqd->sfqq_size = 0;
  // sfqd->sfqq_total_seek = 0;
  sfqd->sfqr_size = 0;
  sfqd->heap_size = 0;
  sfqd->heap_limit_size = 0;
  sfq_pool = KMEM_CACHE(sfq_request, 0);
  INIT_LIST_HEAD(&sfqd->queue);
  sfqd->rq_queue = q;
  INIT_WORK(&sfqd->unplug_work, sfq_kick_queue);
  hrtimer_init(&sfqd->idle_slice_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  sfqd->idle_slice_timer.function = sfq_idle_slice_timer;

  sfqd->chara_locker = 0;
  sfqd->frisk_locker = 1;

  sfqd->wrong_sfqq = NULL;

	// INIT_LIST_HEAD(&sfqd->noop_queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

  printk("INIT_SFQ WRITING_ERROR: %d BETA\n",REQUEST_DEPTH);

	return 0;
}

static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
  struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_queue *sfqq;
  struct sfq_request *sfqr;
  struct list_head *head;

  if(!sfqd || !rq || !bio) return 0;

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

   // allocate sfq_queue
  sfqq = (struct sfq_queue*)kmalloc(sizeof(struct sfq_queue), gfp_mask);
  sfqq->pid = current->pid;
  sfqd->sfqq_size++;
  INIT_LIST_HEAD(&sfqq->queue);
  rq->elv.priv[0] = sfqq;
  // add to sfq_data
  list_add_tail(&sfqq->queuelist, &sfqd->queue);
  // printk("SET_REQUEST PID: %d heap_limit_size: %d\n", current->pid, sfqd->heap_limit_size);

  skip_sfq_queue:
  rcu_read_lock();
  sfqr = kmem_cache_alloc_node(sfq_pool, GFP_NOWAIT | __GFP_ZERO, sfqd->rq_queue->node);
  rcu_read_unlock();
  sfqr->start_tag = 0;
  rq->elv.priv[1] = sfqr;
  return 0;
}


static void sfq_add_request(struct request_queue *q, struct request *rq)
{
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

  // printk("ADD_REQUEST: %d PID: %d\n",sfqr->start_tag, sfqq->pid);
  list_add_tail(&sfqr->queuelist, &sfqq->queue);
  sfqd->sfqr_size++;

}



static int sfq_dispatch(struct request_queue *q, int force)
{
  struct sfq_data *sfqd = q->elevator->elevator_data;
  // struct list_head *head;
  struct sfq_queue *sfqq, *sfqq_test;
  struct sfq_request *sfqr;
  struct request *rq;
  int index;
  u64 sl = 0;




  hrtimer_start(&sfqd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
  // hrtimer_try_to_cancel(&sfqd->idle_slice_timer);

  // dispatched for the first time
  if(!(sfqd->os_sfqq)){
      sfqq = sfqd->os_sfqq = list_first_entry_or_null(&sfqd->queue, struct sfq_queue, queuelist);
      if(!(sfqq)) return 0;
      // sfqd->sfqq_total_seek++;

      // printk("FIRST_DISPATCH PID: %d SEEK: %d  SIZE: %d\n", sfqd->os_sfqq->pid, sfqd->sfqq_total_seek, sfqd->sfqq_size);

  // currently dispatching
  } else {
  next_dispatch:

      sfqq = list_next_entry(sfqd->os_sfqq, queuelist);
      sfqd->os_sfqq = sfqq;
        if(!sfqd->wrong_sfqq){
              sfqq_test = list_next_entry(sfqq, queuelist);
              // printk("prev: %d, next: %d\n",sfqq->pid, sfqq_test->pid);
              sfqd->wrong_sfqq = sfqq_test;
              goto next_dispatch;
        } else {
          if(sfqq->pid == sfqd->wrong_sfqq->pid){
                  // printk("pid: %d caught\n", sfqq->pid);
                  // sfqd->sfqq_total_seek = 0;
                  goto next_dispatch;
          }
        }


      // check for left requests
      if(list_empty(&sfqd->os_sfqq->queue)){
          // printk("kblockd_schedule_work 1\n");
          if(sfqd->sfqr_size && sfqd->chara_locker){
              // printk("kblockd_schedule_work 1\n");
               kblockd_schedule_work(&sfqd->unplug_work);
          }
          // if empty proceed to the heap-queue dispatch
          goto dispatch_section;
      }
       // printk("NEXT_DISPATCH PID: %d SEEK: %d  SIZE: %d\n", sfqd->os_sfqq->pid, sfqd->sfqq_total_seek, sfqd->sfqq_size);
  }

  // limiting number of outstanding requests
  if(sfqd->depth > REQUEST_DEPTH){

    if(sfqd->sfqr_size){ //&& sfqd->chara_locker){
        kblockd_schedule_work(&sfqd->unplug_work);
    }
     return 0;
  }


  // if(!sfqd->frisk_locker) goto dispatch_section;

  // transfer requests into heap-queue
  sfqr = list_first_entry_or_null(&sfqd->os_sfqq->queue, struct sfq_request, queuelist);
  if(sfqr){
    // printk("\t\tRQUEST to HEAP_QUEUE: %d PID: %d\n", sfqr->start_tag,sfqd->os_sfqq->pid);
    // remove request from sfq_queue
    if(sfqd->heap_size < sfqd->heap_limit_size-1){

        list_del_init(&sfqr->queuelist);
        // printk("\t\tAdded to HEAP_QUEUE: %d PID: %d\n", sfqr->start_tag,sfqd->os_sfqq->pid);
        index = (sfqd->heap_size)++;
        while(index && sfqr->start_tag < sfqd->requests[PARENT(index)]->start_tag) {
            sfqd->requests[index] = sfqd->requests[PARENT(index)] ;
            index = PARENT(index);
        }
        sfqd->requests[index] = sfqr;
    } else{
        if(sfqd->sfqr_size){
            // printk("kblockd_schedule_work 3\n");
            // kblockd_schedule_work(&sfqd->unplug_work); chara
        }
    }
  }

  // perform the heap-sort
  dispatch_section:
  if(sfqd->heap_size>0){
      sfqr = sfqd->requests[0];
      rq = sfqr->rq;
      if(rq) elv_dispatch_sort(q, rq);

      sfqd->requests[0] = sfqd->requests[--(sfqd->heap_size)];
      if(sfqd->heap_size>1) heapify(sfqd, 0);
      // printk("DISPATCH: %d PID: %d \n", sfqr->start_tag, sfqd->os_sfqq->pid);
      // printk("DISPATCH_FINISHED: %d PID: %d\n", sfqr->start_tag,sfqd->os_sfqq->pid);

      sfqd->depth++;
      return 1;
  }


  // something wrong haha
  // sfqr = list_first_entry_or_null(&sfqd->os_sfqq->queue, struct sfq_request, queuelist);
	// if (sfqr) {
  //   // printk("ERROR DISPATCH: %d PID: %d\n", sfqr->start_tag,sfqd->os_sfqq->pid);
  //   list_del_init(&sfqr->queuelist);
	// 	elv_dispatch_sort(q, sfqr->rq);
  //   sfqd->depth++;
	// 	return 1;
	// }

	return 0;
}


static void sfq_completed(struct request_queue *q, struct request *rq){
  struct sfq_data *sfqd = q->elevator->elevator_data;
  // u64 sl = 0;
  sfqd->depth--;

  // printk("COMPLETE BEFORE PID: %d sfqr_size %d\n", sfqd->os_sfqq->pid, sfqd->sfqr_size);
  hrtimer_try_to_cancel(&sfqd->idle_slice_timer);

  if((--sfqd->sfqr_size)>0){
     // invoke the dispatch again
     // hrtimer_start(&sfqd->idle_slice_timer, ns_to_ktime(sl), HRTIMER_MODE_REL);
     if(sfqd->sfqr_size) kblockd_schedule_work(&sfqd->unplug_work);

     sfqd->chara_locker=1;

  }
}

static void sfq_put_request(struct request *rq){
  struct sfq_request *sfqr = rq->elv.priv[1];
  if(sfqr) kmem_cache_free(sfq_pool, sfqr);
}


static void sfq_exit_queue(struct elevator_queue *e)
{
  struct sfq_data *nd = e->elevator_data;
  // hrtimer_cancel(&nd->idle_slice_timer);
  cancel_work_sync(&nd->unplug_work);
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
