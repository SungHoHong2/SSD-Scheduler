# SSD-Scheduler

### In a Nutshell
1. heap algorithm sorts the queue for requests.
2. virtual_time only uses logical_time
3. limit the number of concurrent I/O requests(Depth)

### Pending Question
```
intensive number of jobs will give me an error
   - suspecting the bad sector problem
   - trying to find whether there is a method of preventing the bad sector.

try out tracking each of the requests by number. and check whether


```
[link to the error ](https://unix.stackexchange.com/questions/43681/kde-causes-read-fpdma-queued-error)


### Depth Performance
- the number of allowed I/O requests that are dispatched
- [Evaluation] check the overall I/O throughput based on the number of allowed Depth
  - [test result of depth](test_results/2017_06_30_depth_results)
- [Evaluation] compare the best performance with the NOOP, CFQ and SFQ
  - [test result of schedulers](test_results/2017_06_30_scheduler_results)


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
