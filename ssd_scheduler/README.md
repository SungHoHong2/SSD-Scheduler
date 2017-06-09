# SSD-scheduler

### Main Features
1. min-heap for sorting the requests
2. SFQ algorithm

### Global Values
- assume all requests have fixed length
- assume all requests are reads
- declare min-heap index functions
- declare sfq_global struct
   - global struct for the elevator
   - include virtual_time that tracks the logical time
   - include requests array for sorting the requests
- declare sfq_request struct
  - include start_tag to assign the arrival time
  - include finish_tag to estimate the finish time
  - include request struct to assign the request   

```

// assume all requests have fixed size
#define REQUEST_LENGTH 100;

// assume all requests are read
#define REQUEST_WEIGHT 1;

// Heap Sort - Indexing
#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2


typedef struct sfq_global{

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
- get_highest_finish_tag function is used to compare the arrival time and the finish_tag before assigning the start_tag to the virtual_time

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

int get_highest_finish_tag(sfq_global *hp, int i) {
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

```


### SFQ INIT QUEUE
1. allocate memory to sfq_global struct
2. assign zero for the current virtual_time
```

static int sfq_init_queue(struct request_queue *q, struct elevator_type *e){

  struct sfq_global *sfqg;
  struct elevator_queue *eq;

  eq= elevator_alloc(q, e);
  if(!eq)
    return -ENOMEM;

  sfqg = kzalloc_node(sizeof(*dd), GFP_KERNEL, q->node);
  if(!dd){
      kobject_put(&eq->kobj);
      return -ENOMEM;
  }
  eq->elevator_data = sfqg;

  // initialize virtual time
  sfqg->virtual_time = 0;

  // initialize size of the heap array
  sfqg->size = 0;

  spin_lock_irq(q->queue_lock);
  q->elevator = eq;
  spin_unlock_irq(q->queue_lock);

  return 0;
}

```

### SFQ INIT QUEUE
1. allocate struct (sfq_request)
2. assign pid to each request
3. compare and assign start_tag of each request
4. estimate finish_tag for each request
5. assign the struct for each request privately

```

static int sfq_set_request(struct request_queue *q, struct request *rq, struct bio *bio, gfp_t gfp_mask){
     struct sfq_global *sfqd = q->elevator->elevator_data;
     struct sfq_request *sfqr;
     int latest_finish_tag;

     sfqr = (struct zfq_queue*)kmalloc(sizeof(struct zfq_queue), gfp_mask);
     sfqr->pid = current->pid;


     // update the virtual_time with the smallest outstanding request
     if(sfqd->size>0 && sfqd->elem_test[0])
     sfqd->virtual_time = sfqd->elem_test[0]->data;


     // compare the time with latest request
     latest_finish_tag = get_highest_finish_tag(sfqd, 0);
     if(sfqd->virtual_time < latest_finish_tag){
       sfqr->start_tag = latest_finish_tag;
     } else {
       sfqr->start_tag = sfqd->virtual_time;
     }

     // estimate the finish_tag of the request
     sfqr->finish_tag = sfqr->start_tag + ( REQUEST_LENGTH / REQUEST_WEIGHT );

     rq->elv.priv[0] = sfqr;
}

```

### SFQ ADD REQUEST
1. assign request pointer to each sfq_request struct
2. add requests in the global array obligating to heap-sort policy
3. increase the size of the array for each requests

```

static void sfq_add_request(struct request_queue *q, struct request *rq){
    struct sfq_global *sfqd = q->elevator->elevator_data;
    struct sfq_request *sfqr = (struct sfq_request *)(rq->elv.priv[0]);
    int i;

     // assign the request -> sfq_request struct
     sfqr->rq = rq;


     // assign the sfq_request struct in array (heap-sorting)
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
     sfqd->requests[i] = sfqr ;

}

```

### SFQ DISPATCH REQUESTS
1. dispatch the request with the smallest start_tag
2. remove the dispatched request from the array
3. decrease the length of the array
4. secure the heap-sort policy by using the heapify function   

```

static int sfq_dispatch_requests(struct request_queue *q, int force){
  struct sfq_global *sfqd = q->elevator->elevator_data;
  struct request *rq;

  if(sfqd->size) {

      //dispatch request
      rq = sfqd->requests[0]->rq;
      elv_dispatch_sort(q, rq)

      // remove the dispatched request from the heap-sort array
      sfqd->requests[0] = sfqd->requests[--(sfqd->size)] ;
      sfqd->requests = (node **)krealloc(sfqd->requests, sfqd->size * sizeof(node *), GFP_KERNEL) ;
      heapify(hp, 0) ;
  }
    return 0;
}

```


### SFQ EXIT QUEUE
1. free the sfq_global struct

```
static void sfq_exit_queue(struct elevator_queue *e){
	struct sfq_global *sfqd = e->elevator_data;
	kfree(sfqd);
}
```
