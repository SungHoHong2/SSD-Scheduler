# SSD-Scheduler

### Insights
1. the latency may be determined by fifo_batch.
   - the size of the batch determined by the sets of the increasing sector number
   - the smaller batch can reduce latency by ensuring new requests are exectued sooner.
   - fifo batch can determine the depth
     ```
     if(rq && dd->batching < dd->fifo_batch)
     goto dispatch_request;
     ```

### Rules of SFQ
1. only need one queue for all pending requests
2. each requests track its PID and assign it's start_time
3. keep the queues sorted according to start_time
   - need data structure that supports efficient sorting, such as heap
4. when a new request comes or dispatch requests is finished
   - determine the start/finish time
   - dispatch a request (if depth has not been reached)
   - advance the virtual time

### Experiment with Deadline-Scheduler
1. deadline queues are basicaly sorted by their deadline (expiration time) while the sorted queues are sorted by the sector number
2. The main goal of the deadline scheduler is to guarantee a start service time for a request. It does so by imposing a deadline on all I/O operations to prevent starvation of request.
3. Before serving the next request, the deadline scheduler decides which queue is to use. Read queues are given a higher priority, because processes usually block on read operations.
4. next the deadline scheduler checks if the first request in the deadline has expired, otherwise the scheduler serves a batch of requests from the sorted queue.
5. read requests have an expiratio time of 500 ms, and write requests expire 5 seconds
