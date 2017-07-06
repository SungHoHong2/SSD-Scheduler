#### Generic Block Layer
- The generic block layer is an abstraction for block devices[HDD, SSD] in the system
- These block devices may be physical or logical
- Receives I/O requests in a queue, and is responsible for passing them along to block devices
- The I/O requests are scheduled to be submitted to the devices by an algorithm called an I/O scheduler


#### I/O Schedulers
- maximize the block i/O performance
- schedulers operate on pending requests
  - Merge requests for adjacent sectors
  - Sort pending requests based on LBA
  - Time when requests are submitted
  - Must minimize latency, maximize throughput, and avoid starvation


#### Analysis of Elevator-Framework
 - elevator.h has the interface functions that are overlapped with the used-schedulers
   - cfq, noop etc
 - actual functions used in elevator.c are only register and unregister


#### Analysis of CFQ Scheduler
- Per-process sorted queues for sync requests
- Fewer queues for async requests
- Allocates time slice for each queue
- Priorities are taken into account
- may idle if quantum not expired - anticipation
