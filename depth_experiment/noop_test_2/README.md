### Depth Experiment with NOOP-Scheduler  
- tested with FIO
  - FIO's job number more than 7 **-> error occurs**


```
fio -filename=/dev/sdb -direct=1 -thread -rw=read -bs=4k -numjobs=7 -name=mytest
```

#### Dispatch Function
1. **[MODIFIED]** check whether the complete_flag is zero [already dispatched]
   - if is, return 0   
2. check whether the value of depth is within the limited number
   - if not, return 0    
3. select the first I/O request from the linked-list   
4. remove the selected I/O request from the linked-list
5. dispatch the request
6. **[MODIFIED]** update the complete_flag to zero [already dispatched]
7. increase the depth value

```c

static int noop_dispatch(struct request_queue *q, int force){
  static int noop_dispatch(struct request_queue *q, int force){
  	struct noop_data *nd = q->elevator->elevator_data;
  	struct request *rq;

      if(!nd || nd->complete_flag==0 || nd->depth>REQUEST_DEPTH){
  				return 0;
      }

  		rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);

  		if (rq) {
  		   printk("DISPATCH flag: %d  depth: %d\n",nd->complete_flag, nd->depth);
  			 list_del_init(&rq->queuelist);
  			 elv_dispatch_sort(q, rq);

  	       nd->depth++;
  			 nd->complete_flag = 0;
  			 return 1;
  		}

  	return 0;
  }

```


#### Completed Function
1. **[MODIFIED]** update the complete_flag to one [ready to dispatch]
2. decrease the number when the I/O request is completed

```c

static void noop_completed(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;

		  printk("COMPLETE flag: %d  depth: %d\n",nd->complete_flag, nd->depth);
          nd->complete_flag = 1;
			nd->depth--;

}


```
