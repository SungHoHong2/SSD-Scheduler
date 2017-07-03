# SSD-Scheduler

### In a Nutshell
1. heap algorithm sorts the queue for requests.
2. virtual_time only uses logical_time
3. limit the number of concurrent I/O requests(Depth)

### Question
```
Check whether the scheduler runs without heapify
and then check whether it runs with depth 1
-> if this works the depth problem is due to too many recursion

[A] Currently I have tried with depth 1 without the heapify (the main reason of recursion)
    - but the FIO wont work
[A] Attempt to try with the noop
    - dont work with 64jobs but yes it works with 32 jobs with 1 depth

[A] random read & write works with 1 depth

[C] So the reason to this is that there is a limit that the schduler limits the number of recursion that can be handled within the I/O Scheduler.
So If below 1 depth is desired, we may be able to tackle this problem by limiting the number of heap and place the rest of the requests in the linked-list.

[C] check whether the min-heap array causes a problem in the noop because I don't think it causes
problem. check whether fully stacked up number causes the problem

```


### Depth Performance
- the number of allowed I/O requests that are dispatched
- [Evaluation] check the overall I/O throughput based on the number of allowed Depth
  - [test result of depth](2017_06_30_depth_results)
- [Evaluation] compare the best performance with the NOOP, CFQ and SFQ
  - [test result of schedulers](2017_06_30_scheduler_results)


### Rules of SFQ
1. only need one queue for all pending requests
2. each requests track its PID and assign it's start_time
3. keep the queues sorted according to start_time
   - need data structure that supports efficient sorting, such as heap
4. when a new request comes or dispatch requests is finished
   - determine the start/finish time
   - dispatch a request (if depth has not been reached)
   - advance the virtual time
5. virtual time does not use jiffies
   - only useus the start_time and the finish_time
   - start_time and finish_time are only assigned with the size of the requests.


### Applying SFQ and Heap-Sort in SSD Scheduler
1. pseudo code for SSD scheduler [view](ssd_scheduler/README.md)
2. source-code (min-heap with start_tag) [view](ssd_scheduler/sfq-sched_6_complete.c)
3. updated source-code (fixed errors) [view](ssd_scheduler/sfq-sched_9_complete.c)


### Experiment with Heap-Sort
1. heap sort is recommended to be sorted in an array (not a linked-list)
   - heap sort is good for big O and in-place.
   - using linked-list will break the big O because it relies on random access to the array
2. implementation of min-heap
   - userspace implementation [view](heap_experiments/min_heap_3.c)
   - kernelspace implementation [view](heap_experiments/min_heap_4.c)

### Experiment with Deadline-Scheduler
1. deadline queues are basicaly sorted by their deadline (expiration time) while the sorted queues are sorted by the sector number
2. The main goal of the deadline scheduler is to guarantee a start service time for a request. It does so by imposing a deadline on all I/O operations to prevent starvation of request.
3. Before serving the next request, the deadline scheduler decides which queue is to use. Read queues are given a higher priority, because processes usually block on read operations.
4. next the deadline scheduler checks if the first request in the deadline has expired, otherwise the scheduler serves a batch of requests from the sorted queue.
5. read requests have an expiratio time of 500 ms, and write requests expire 5 seconds
