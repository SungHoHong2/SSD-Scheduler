
// (?) restricted parallelism
   // grant fairness to the write? by porportion of the concurrent requests?

// (?) Anticipatory Idlenss? -> need to check how it is implemented before making decisions,

struct sfq_data{
  struct rb_root sort_list;     // sorting the start_tag
  struct sfq_request *next_rq;  // current request
  int virtual_time;             // virtual time

  unsigned int batching;        // number of allowed concurrent requests (depth)
  // unsigned int write_starved;   // checking for the fairness of the writes

  int current_batch;
  // int writes_starved;
};


struct sfq_request{
  pid_t pid;
  struct request *rq;
  int start_tag;
  int finish_tag;
};

#define current_virtual_time (jiffies*1000) / HZ;

// exit_queue
   // check all of the requests are removed from the queue
   // free the global data

static void exit_queue(struct elevator_queue *e){
  struct sfq_data *sfqd = e->elevator_data;
  kfree(sfqd);
}

static void completed_req_fn(struct request_queue *q, struct request *rq){
	struct sfq_data *sfqd = q->elevator->elevator_data;
  sfqd->virtual_time = current_virtual_time;
}

static int dispatch_requests(struct request_queue *q){

    // check the avialble number of depth before dispatch the requests
    if(rq && sfqd->batching > sfqd->fifo_batch) return 0;

    // call the sfq_request with the minimum start_tag
    struct rb_node *n = root->rb_node;
    sfq_request* sfqr;
    while(n){
     sfqr = rb_entry(n, MYDATA*, rb_node);
     if(virtual_time < sfqr->start_tag)
         n = n->rb_left;
     else if(virtual_time > sfqr->start_tag)
         n = n->rb_right;
     else
         break;
    }

    // increment the current depth
    sfqd->batching++;

    // rmove the selected request from the rb_tree
    sfqd_del_rq_rb(dd, sfqr)

    // dispatch the request
    elv_dispatch_add_tail(q, sfqr->rq);
}



 static void add_request(struct request_queue *q, struct request *rq){
   struct deadline_data *sfqd = q->elevator->elevator_data;

   // add the request to rbtree for sorting the start_tag
   struct rb_root = sfqd->sort_list;
   struct sfq_rquest *sfqr = rq->elv.priv[0];
   sfqr->rq = rq;

   start_tag_rb_add(rb_root, sfqr);
 }


static int set_request(struct request_queue *q, struct request *rq){
  struct sfq_data *sfqd = q->elevator->elevator_data;
  struct sfq_request *sfqr;

  // allocate sfq's private struct
  sfqr = (struct sfq_request*)kmalloc(sizeof(struct zfq_queue), gfp_mask);
  sfqr->pid = current->pid;

  // initialize the start_tag time
  sfqr->start_tag = current_virtual_time;

  // compare current arrival_time with the latest finish tag
  if(!list_empty(&sfqd->fifo_list)){
      if(virtual_time > sfqr->start_tag){
         sfqr->start_tag = virtual_time; // assign the highest value in the start_tag time
      }
  }

  //(Q) do we calculate the length of the flow with the sector size?
  // sector number : rq->__sector
  // read or write : const int data_dir = rq_data_dir(rq);
  sfqr->finish_tag = sfqr->start_tag + (length of the flow / weight of the flow);

  rq->elv.priv[0] = sfqr;
  return 0;
}



static int init_queue(struct request_queue *q, struct elevator_type *e){

    // initialize the elevator
       struct sfq_data *sfqd;
       eq = elevator_alloc(e);
       sfqd = kzalloc_node(sizeof(*dd), GFP_KERNEL, q->node);
       eq->elevator_data = sfqd;

    // initialize sort_list for requests
       sfqd->sort_list = RB_ROOT;

    // initialize the virtual time to zero
       sfqd->virtual_time = 0;

    // initialize the depth(number of concurrent requests available) from off-line
       (?) Depth of next period = current depth + K*(reference latency * previous latency)
       sfqd->batching = 5;


       spin_lock_irq(q->queue_lock);
       q->elevator = eq;
       spin_unlock_irq(q->queue_lock);
}


static struct elevator_type scheduler = {
    .ops = {
      // configure the necessary data
    }
}

static int __init(void){
   // start the elevator when the kernel module is enabled
   return elv_register();
}

static void __exit(void){
  // exit the elevator when the kernel module is disabled
    elv_unregister();
}
