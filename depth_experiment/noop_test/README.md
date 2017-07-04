### Depth Experiment with NOOP-Scheduler  
- tested with FIO
- test subject is NOOP-Scheduler
- scheduler's depth [I/O request] is limited to **[1 ~ 27] -> error occurs**
- scheduler's depth [I/O request] is limited to **[28 ~ ] -> fine**

```
fio -filename=/dev/sdb -direct=1 -thread -rw=read -bs=4k -numjobs=32 -name=mytest

```

#### Dispatch Function
1. **[MODIFIED]** check whether the value of depth is within the limited number
   - if not return 0    
2. select the first I/O request from the linked-list   
3. remove the selected I/O request from the linked-list
4. dispatch the request
5. **[MODIFIED]** increase the depth value

```c

static int noop_dispatch(struct request_queue *q, int force){
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

  if(nd && nd->depth>REQUEST_DEPTH) return 0; // MODIFIED

        printk("DISPATCH: depth: %d\n",nd->depth);

      	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
      	if (rq) {
      		list_del_init(&rq->queuelist);
      		elv_dispatch_sort(q, rq);

          nd->depth++; //MODIFIED

      		return 1;
      	}
	return 0;
}
```


#### Completed Function
1. **[MODIFIED]** decrease the number when the I/O request is completed

```c

static void noop_completed(struct request_queue *q, struct request *rq){
	struct noop_data *nd = q->elevator->elevator_data;
  printk("COMPLETE: depth: %d\n",nd->depth);
  nd->depth--; //MODIFIED
}
```
