### CFQ Scheduler
- find how scheduler can invoke dispatch right after the complete function
- as a result we can ensure the sequence of dispatch & complete

#### Hint about ICQ
- io_cq (icq) is association between an io_context (ioc) and a request_queue (q).
- http://elixir.free-electrons.com/linux/latest/source/include/linux/iocontext.h#L71


#### Saving attributes in the CFQ
1. declare the attributes using function MACRO
2. send it to the elevator as an array


##### - CFQ INIT QUEUE
1. allocate elevator object
2. allocate cfq_data
3. assign cfq_data to elevator
4. allocate cfq_rb_root
   - rb_root
5. allocate cfq_group
  -  rb_node
6. initialize root_group
  - clear node and initialize the time
  - assign weight
7. allocate prior_trees
   - two rb_roots
8. allocate oom_cfq_queue
   - static class for cfq_queue
   - link the cfq_queue with cfq_group
9. assign hrtimer in cfq_data
   - idle_slice_timer
10. check for active_queue in cfq_data
   - restart the hrtimer
   - assign it in idle_slice_timer.function  
11. initialize work for request_queue
   - request_queue holds the cfq_data
12. initialize rest for the cfq_data attributes
