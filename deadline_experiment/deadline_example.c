#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/rbtree.h>

static const int read_expire = HZ / 2;
static const int write_expire = 5 * HZ;
static const int writes_starved = 2; // max times of read can starve a write
static const int fifo_batch = 16;   // sequential requests treated as one

struct deadline_data{

  // requests are present on both sort_list and fifo_lists
  struct rb_root sort_list[2];
  struct list_head fifo_list[2];

  struct request *next_rq[2];
  unsigned int batching; //number of sequential requests made
  unsigned int starved;  // times read have starved writees

  //settings that change how the sheduler behaves
  int fifo_expire[2];
  int fifo_batch;
  int writes_starved;
  int front_merges;

};


static inline struct rb_root *deadline_rb_root(struct deadline_data *dd, struct request *rq){
  return &dd->sort_list[rq_data_dir(rq)];
}

// get the request after 'rq' in sector-sorted order
static inline struct request *deadline_latter_request(struct request *rq){
  struct rb_node *node = rb_next(&rq->rb_node);

  if(node)
    return rb_entry_rq(node);
  return NULL;
}

static void  deadline_add_rq_rb(struct deadline_data *dd, struct request *rq){
  struct rb_root *root = deadline_rb_root(dd, rq);

  // add the node baed on the sector number
  elv_rb_add(root, rq);
}

static inline void deadline_del_rq_rb(struct deadline_data *dd, struct request *rq){
  const int data_dir = rq_data_dir(rq);

  if(dd->next_rq[data_dir] == rq){
    dd->next_rq[data_dir] = deadline_latter_request(rq);
  }
  elv_rb_del(deadline_rb_root(dd, rq), rq);
}

static void deadline_move_to_dispatch(struct deadline_data *dd, struct request *rq){
  struct request_queue *q = rq->q;

  // deadline_remove_request(q, rq);
  rq_fifo_clear(rq);
  deadline_del_rq_rb(dd, rq);

  elv_dispatch_add_tail(q, rq);
}

static void deadline_move_request(struct deadline_data *dd, struct request *rq){
  const int data_dir = rq_data_dir(rq);

  dd->next_rq[READ] = NULL;
  dd->next_rq[WRITE] = NULL;
  dd->next_rq[data_dir] = deadline_latter_request(rq);

  deadline_move_to_dispatch(dd, rq);
}

static inline int deadline_check_fifo(struct deadline_data *dd, int ddir){
  struct request *rq = rq_entry_fifo(dd->fifo_list[ddir].next);

  if(time_after_eq(jiffies, (unsigned long) rq->fifo_time))
    return 1;

  return 0;
}


static int deadline_dispatch_requests(struct request_queue *q, int force){
  struct deadline_data *dd = q->elevator->elevator_data;
  const int reads = !list_empty(&dd->fifo_list[READ]);
  const int writes = !list_empty(&dd->fifo_list[WRITE]);
  struct request *rq;
  int data_dir;


  // invoke current read or right
  if(dd->next_rq[WRITE]){
      rq = dd->next_rq[WRITE];
  }else{
      rq = dd->next_rq[READ];
  }

  if(rq && dd->batching < dd->fifo_batch)
  goto dispatch_request;


  // when the request is read
  if(reads){
    BUG_ON(RB_EMPTY_ROOT(&dd->sort_list[READ]));

    if(writes && (dd->starved++ >= dd->writes_starved))
      goto dispatch_writes;

    data_dir = READ;
    goto dispatch_find_request;

  }

  // when the request is write
  if(writes){
dispatch_writes:
    BUG_ON(RB_EMPTY_ROOT(&dd->sort_list[WRITE]));
    dd->starved = 0;
    data_dir = WRITE;

    goto dispatch_find_request;
  }

dispatch_find_request:
    if(deadline_check_fifo(dd, data_dir) || !dd->next_rq[data_dir]){
      rq = rq_entry_fifo(dd->fifo_list[data_dir].next);
    } else {
      rq = dd->next_rq[data_dir];
    }
    dd->batching = 0;

dispatch_request:
    dd->batching++;
    deadline_move_request(dd, rq);


 return 0;
}


static void deadline_add_request(struct request_queue *q, struct request *rq){
  struct deadline_data *dd = q->elevator->elevator_data;

  // check whether the request is write or read
  const int data_dir = rq_data_dir(rq);
  deadline_add_rq_rb(dd, rq);

  // set expire time
  rq->fifo_time = jiffies + dd->fifo_expire[data_dir];
  list_add_tail(&rq->queuelist, &dd->fifo_list[data_dir]);
}

static void deadline_exit_queue(struct elevator_queue *e){
  struct deadline_data *dd = e->elevator_data;
  BUG_ON(!list_empty(&dd->fifo_list[READ]));
  BUG_ON(!list_empty(&dd->fifo_list[WRITE]));
  kfree(dd);
}

static int deadline_init_queue(struct request_queue *q, struct elevator_type *e){

  struct deadline_data *dd;
  struct elevator_queue *eq;

  /*
   * Initialize elevator
   */

  eq= elevator_alloc(q, e);
  if(!eq)
    return -ENOMEM;

  dd = kzalloc_node(sizeof(*dd), GFP_KERNEL, q->node);
  if(!dd){
      kobject_put(&eq->kobj);
      return -ENOMEM;
  }
  eq->elevator_data = dd;

  // initialize listhead for each read and write
  INIT_LIST_HEAD(&dd->fifo_list[READ]);
  INIT_LIST_HEAD(&dd->fifo_list[WRITE]);

  // initialize sort_list for reach read and write
  dd->sort_list[READ] = RB_ROOT;
  dd->sort_list[WRITE] = RB_ROOT;

  // fifo expire is an int each given to read and write
  // allocate const int read_expire
  dd->fifo_expire[READ] = read_expire;
  dd->fifo_expire[WRITE] = write_expire;

  dd->writes_starved = writes_starved;
  dd->fifo_batch = fifo_batch;

  spin_lock_irq(q->queue_lock);
  q->elevator = eq;
  spin_unlock_irq(q->queue_lock);

  return 0;
}



static struct elevator_type iosched_deadline = {
  .ops = {
      .elevator_dispatch_fn = deadline_dispatch_requests,
      .elevator_add_req_fn = deadline_add_request,
      .elevator_init_fn = deadline_init_queue,
      .elevator_exit_fn = deadline_exit_queue,
  },

  // .elevator_attrs = deadline_attrs,
  .elevator_name = "zfq",
  .elevator_owner = THIS_MODULE,
};

static int __init deadline_init(void){
	return elv_register(&iosched_deadline);
}

static void __exit deadline_exit(void){
  elv_unregister(&iosched_deadline);
}


module_init(deadline_init);
module_exit(deadline_exit);

MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Deadline experiment");
