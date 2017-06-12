# SSD-scheduler

### Main Features
1. min-heap for sorting the requests
2. SFQ algorithm

### Global Values
- assume all requests have fixed length
- assume all type of requests is READ (fixed weight)
- declare indexing functions for min-heap
- declare sfq_global struct
   - virtual_time: tracks the logical time
   - requests[array]: array for sorting the requests
- declare sfq_request struct
  - start_tag: assign the arrival time
  - finish_tag: estimate the finish time
  - request struct: assign the request   

```
// assume all requests have fixed size
#define REQUEST_LENGTH 100;

// assume all requests are read
#define REQUEST_WEIGHT 1;

// Heap Sort - Indexing
#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2


typedef struct sfq_data{

    // SFQ Algorithm
    int virtual_time;

    // Heap Sort
    int size ;
    sfq_request **requests;

} sfq_global;


typedef struct sfq_request {

    // SFQ Algorithm
    int start_tag;
    int finish_tag;

    pid_t pid;
    struct request *rq;

} sfq_request;
```

### Common Functions for Heap Sorting
- heapify function checks violation of the heap property after removing a request from the array   

```
/*
 * Heap Sorting Common Function
 */

void heap_swap(sfq_request *n1, sfq_request *n2) {
    sfq_request temp = *n1 ;
    *n1 = *n2 ;
    *n2 = temp ;
}

// find the smallest node at index i by comparing parent, left and right child
void heapify(sfq_global *hp, int i) {
    int smallest = (LCHILD(i) < hp->size && hp->requests[LCHILD(i)]->start_tag < hp->requests[i]->start_tag) ? LCHILD(i) : i ;
    if(RCHILD(i) < hp->size && hp->requests[RCHILD(i)]->start_tag < hp->requests[smallest]->start_tag) {
        smallest = RCHILD(i) ;
    }
    if(smallest != i) {
        heap_swap(hp->requests[i], hp->requests[smallest]) ;
        heapify(hp, smallest) ;
    }
}
```


### SFQ INIT QUEUE
1. allocate memory for sfq_global struct
2. assign virtual_time to zero
```
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


    spin_lock_irq(q->queue_lock);
    q->elevator = eq;
    spin_unlock_irq(q->queue_lock);

    printk("INIT_SFQ SCHEDULER\n");
    return 0;
}

```

### SFQ SET REQUEST
1. allocate memory for struct(sfq_request)
2. assign pid for each request
3. compare and assign start_tag of each request
4. estimate finish_tag for each request
5. assign the struct(sfq_request) to each request

```
static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
    struct sfq_data *sfqd = q->elevator->elevator_data;
    struct sfq_request *sfqr;
    int latest_finish_tag;

    sfqr = (struct sfq_request*)kmalloc(sizeof(struct sfq_request), gfp_mask);
    sfqr->pid = current->pid;
    sfqr->valid = 1;

    sfqr->start_tag = sfqd->virtual_time;

    // update the virtual_time with the smallest outstanding request
    if(sfqd && sfqd->size>0 && sfqd->requests[0] && sfqd->requests[0]->start_tag >=0){
      // sfqd->virtual_time = sfqd->requests[0]->start_tag;
       latest_finish_tag = sfqd->requests[(sfqd->size)-1]->finish_tag;
       if(sfqd->virtual_time < latest_finish_tag){
             sfqd->virtual_time = latest_finish_tag;
       }

       sfqr->start_tag = sfqd->virtual_time;
    }

    sfqr->finish_tag = sfqr->start_tag + ( REQUEST_LENGTH / REQUEST_WEIGHT );
    rq->elv.priv[0] = sfqr;

}
```

### SFQ ADD REQUEST
1. assign request address to each sfq_request struct
2. add requests to global array for heap-sort
3. reallocate the size of the array for each requests

```
static void sfq_add_request(struct request_queue *q, struct request *rq){
    struct sfq_data *sfqd = q->elevator->elevator_data;
    struct sfq_request *sfqr = rq->elv.priv[0];
    int i;


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
}
```

### SFQ DISPATCH REQUESTS
1. dispatch the request with the smallest start_tag
2. remove the dispatched request from the array
3. decrease the length of the array
4. secure the heap-sort policy by using the heapify function   

```
static int sfq_dispatch_requests(struct request_queue *q, int force){
    struct sfq_data *sfqd = q->elevator->elevator_data;
    struct sfq_request *sfqr;
    struct request *rq;

    if(sfqd && sfqd->size>0){
          sfqr = sfqd->requests[0];
          rq = sfqr->rq;

          // remove the dispatched request from the heap-sort array
          sfqd->requests[0] = sfqd->requests[--(sfqd->size)] ;
          sfqd->requests = (sfq_request **)krealloc(sfqd->requests, sfqd->size * sizeof(sfq_request *), GFP_KERNEL) ;
          heapify(sfqd, 0) ;

          // dispatch request
          elv_dispatch_sort(q, rq);
          return 1;
    }
  	return 0;
}
```


### SFQ EXIT QUEUE
1. free the memory of the global array storing the requests
2. free the memory of the sfq_global struct

```
static void sfq_exit_queue(struct elevator_queue *e){
    struct sfq_data *nd = e->elevator_data;
    kfree(nd);
}
```
