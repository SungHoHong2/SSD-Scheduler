# SSD-Scheduler

### In a Nutshell
1. heap algorithm sorts the queue for requests.
2. virtual_time only uses logical_time
3. limit the number of concurrent I/O requests(Depth)



### Depth Performance
- the number of allowed I/O requests that are dispatched
- [Evaluation] check the overall I/O throughput based on the number of allowed Depth
  - 4KB read 32jobs
  - 4KB write 32jobs
  - 128KB read 32jobs
  - 128KB write 32jobs
  - The IO patterns mentioned below (mixed read & write with 32jobs)  
  - [test result data](2017_06_30_depth_results)
- [Evaluation] compare the best performance with the NOOP, CFQ and SFQ
  - The IO patterns mentioned below (mixed read & write with 32jobs)    



### FIO parameters used in the Evaluation
1. direct:1 non-buffered I/O, involves reading and writing data one element at a time. since all data accesses are resolved by the I/O device immediatedly, there is no disagree with the data actually in the storage.
2. direct:0 buffered I/O, reading or writing the data in chunks. The entire buffer is written to the device at once.
3. thread: Fio defualts to forking jobs, however if this option is given fio will use POSIX threads, pthread_create(3) instead of forking processes
4. runtime, tell fio to terminate processing after the specified period of time.


### Evaluating the Scheduler
1. IO slowdown ratio
   - average IO latency normalized to that when running alone.
   - each task should experience a factor of n slowdown compared to running-alone.
2. raw data of block size 256k
3. bs (block size)
   - block size for I/O units ex) default 4k
   - bs= int,[int]
   - readers and writers can be specified seperately    
4. bsrange (block size range)
   - specify the range of block size
   - bsrange=1k-4k
5. bssplit
   - allow finer grained control of the block sizes issued. Weight various block sizes for exact control of the issued IO for a job that has mixed block sizes
   - bssplit=4k/10:64k/50:32k/40
   - 50$ 64k blocks, 10% 4kblocks, 40% 32k blocks
   - the format is identicla to what the bs option accepts, the read and write parts are sepearted with a comma     


### Evaluation on Task Fairness
- Scheduler S1 achieves better fairness than scheduler S2, if the slowest task under S1 makes more progress than the slowest task does under S2.


### IO Patterns
- a concurrent run with a reader continuously issuing 4KB reads and a writer continuously issuing 4 KB writes
- a concurrent run with sixteen 4KB readers and sixteen 4KB writers
- a concurrent run with sixteen 4KB readers and sixteen 128KB readers
- a concurrent run with sixteen 4KB writers and sixteen 128KB writers


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
