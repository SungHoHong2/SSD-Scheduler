### CFQ Scheduler


#### Saving attributes in the CFQ
1. declare the attributes using function MACRO
2. send it to the elevator as an array


```
I have no idea why the macros are used and added in the attributes in the cfq scheduler. What is the purpose?
I believe that it is used for tuning the attributes of cfq, but in that case how do we use it?

- currently have asked the stack-overflow

```

##### - DECLARED CFQ ATTRIBUTES

```
static ssize_t
cfq_var_show(unsigned int var, char *page){
 return sprintf(page, "%u\n", var);
}

static ssize_t
cfq_var_store(unsigned int *var, const char *page, size_t count){
 char *p = (char *) page;

 *var = simple_strtoul(p, &p, 10);
 return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 u64 __data = __VAR;						\
 if (__CONV)							\
	 __data = div_u64(__data, NSEC_PER_MSEC);			\
 return cfq_var_show(__data, (page));				\
}

SHOW_FUNCTION(cfq_quantum_show, cfqd->cfq_quantum, 0);
SHOW_FUNCTION(cfq_fifo_expire_sync_show, cfqd->cfq_fifo_expire[1], 1);
SHOW_FUNCTION(cfq_fifo_expire_async_show, cfqd->cfq_fifo_expire[0], 1);
SHOW_FUNCTION(cfq_back_seek_max_show, cfqd->cfq_back_max, 0);
SHOW_FUNCTION(cfq_back_seek_penalty_show, cfqd->cfq_back_penalty, 0);
SHOW_FUNCTION(cfq_slice_idle_show, cfqd->cfq_slice_idle, 1);
SHOW_FUNCTION(cfq_group_idle_show, cfqd->cfq_group_idle, 1);
SHOW_FUNCTION(cfq_slice_sync_show, cfqd->cfq_slice[1], 1);
SHOW_FUNCTION(cfq_slice_async_show, cfqd->cfq_slice[0], 1);
SHOW_FUNCTION(cfq_slice_async_rq_show, cfqd->cfq_slice_async_rq, 0);
SHOW_FUNCTION(cfq_low_latency_show, cfqd->cfq_latency, 0);
SHOW_FUNCTION(cfq_target_latency_show, cfqd->cfq_target_latency, 1);
#undef SHOW_FUNCTION


#define USEC_SHOW_FUNCTION(__FUNC, __VAR)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct cfq_data *cfqd = e->elevator_data;			\
	u64 __data = __VAR;						\
	__data = div_u64(__data, NSEC_PER_USEC);			\
	return cfq_var_show(__data, (page));				\
}
USEC_SHOW_FUNCTION(cfq_slice_idle_us_show, cfqd->cfq_slice_idle);
USEC_SHOW_FUNCTION(cfq_group_idle_us_show, cfqd->cfq_group_idle);
USEC_SHOW_FUNCTION(cfq_slice_sync_us_show, cfqd->cfq_slice[1]);
USEC_SHOW_FUNCTION(cfq_slice_async_us_show, cfqd->cfq_slice[0]);
USEC_SHOW_FUNCTION(cfq_target_latency_us_show, cfqd->cfq_target_latency);
#undef USEC_SHOW_FUNCTION


#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 unsigned int __data;						\
 int ret = cfq_var_store(&__data, (page), count);		\
 if (__data < (MIN))						\
	 __data = (MIN);						\
 else if (__data > (MAX))					\
	 __data = (MAX);						\
 if (__CONV)							\
	 *(__PTR) = (u64)__data * NSEC_PER_MSEC;			\
 else								\
	 *(__PTR) = __data;					\
 return ret;							\
}

STORE_FUNCTION(cfq_quantum_store, &cfqd->cfq_quantum, 1, UINT_MAX, 0);
STORE_FUNCTION(cfq_fifo_expire_sync_store, &cfqd->cfq_fifo_expire[1], 1,
	 UINT_MAX, 1);
STORE_FUNCTION(cfq_fifo_expire_async_store, &cfqd->cfq_fifo_expire[0], 1,
	 UINT_MAX, 1);
STORE_FUNCTION(cfq_back_seek_max_store, &cfqd->cfq_back_max, 0, UINT_MAX, 0);
STORE_FUNCTION(cfq_back_seek_penalty_store, &cfqd->cfq_back_penalty, 1,
	 UINT_MAX, 0);
STORE_FUNCTION(cfq_slice_idle_store, &cfqd->cfq_slice_idle, 0, UINT_MAX, 1);
STORE_FUNCTION(cfq_group_idle_store, &cfqd->cfq_group_idle, 0, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_sync_store, &cfqd->cfq_slice[1], 1, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_async_store, &cfqd->cfq_slice[0], 1, UINT_MAX, 1);
STORE_FUNCTION(cfq_slice_async_rq_store, &cfqd->cfq_slice_async_rq, 1,
	 UINT_MAX, 0);
STORE_FUNCTION(cfq_low_latency_store, &cfqd->cfq_latency, 0, 1, 0);
STORE_FUNCTION(cfq_target_latency_store, &cfqd->cfq_target_latency, 1, UINT_MAX, 1);
#undef STORE_FUNCTION


#define USEC_STORE_FUNCTION(__FUNC, __PTR, MIN, MAX)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
 struct cfq_data *cfqd = e->elevator_data;			\
 unsigned int __data;						\
 int ret = cfq_var_store(&__data, (page), count);		\
 if (__data < (MIN))						\
	 __data = (MIN);						\
 else if (__data > (MAX))					\
	 __data = (MAX);						\
 *(__PTR) = (u64)__data * NSEC_PER_USEC;				\
 return ret;							\
}
USEC_STORE_FUNCTION(cfq_slice_idle_us_store, &cfqd->cfq_slice_idle, 0, UINT_MAX);
USEC_STORE_FUNCTION(cfq_group_idle_us_store, &cfqd->cfq_group_idle, 0, UINT_MAX);
USEC_STORE_FUNCTION(cfq_slice_sync_us_store, &cfqd->cfq_slice[1], 1, UINT_MAX);
USEC_STORE_FUNCTION(cfq_slice_async_us_store, &cfqd->cfq_slice[0], 1, UINT_MAX);
USEC_STORE_FUNCTION(cfq_target_latency_us_store, &cfqd->cfq_target_latency, 1, UINT_MAX);
#undef USEC_STORE_FUNCTION


#define CFQ_ATTR(name) \
	__ATTR(name, S_IRUGO|S_IWUSR, cfq_##name##_show, cfq_##name##_store)

static struct elv_fs_entry cfq_attrs[] = {
	CFQ_ATTR(quantum),
	CFQ_ATTR(fifo_expire_sync),
	CFQ_ATTR(fifo_expire_async),
	CFQ_ATTR(back_seek_max),
	CFQ_ATTR(back_seek_penalty),
	CFQ_ATTR(slice_sync),
	CFQ_ATTR(slice_sync_us),
	CFQ_ATTR(slice_async),
	CFQ_ATTR(slice_async_us),
	CFQ_ATTR(slice_async_rq),
	CFQ_ATTR(slice_idle),
	CFQ_ATTR(slice_idle_us),
	CFQ_ATTR(group_idle),
	CFQ_ATTR(group_idle_us),
	CFQ_ATTR(low_latency),
	CFQ_ATTR(target_latency),
	CFQ_ATTR(target_latency_us),
	__ATTR_NULL
};
```

##### - SEND TO THE ELEVATOR AS AN ARRAY

```
static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_completed_req_fn  = cfq_completed,
		.elevator_dispatch_fn		= cfq_dispatch,
		.elevator_add_req_fn		= cfq_add_request,
		.elevator_init_fn		= cfq_init_queue,
		.elevator_exit_fn		= cfq_exit_queue,
	},
	.elevator_attrs =	cfq_attrs,
	.elevator_name = "cfq_test",
	.elevator_owner = THIS_MODULE,
};
```
