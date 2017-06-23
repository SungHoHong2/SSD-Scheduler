# 1_ I/O benchmarks for evaluation
- explains the four cases of scenarios.
- each benchmark contains a number of tasks issuing I/O requests of different types and sizes

### 4KB with 1 read (running alone)
```
[global]
directory= [ location of the sdb ]
bs=4k
numjobs = 1
group_reporting = 1  
direct=1
rw=read

```

### 4KB with 1 write (running alone)
```
[global]
directory= [ location of the sdb ]
bs=4k
numjobs = 1
group_reporting = 1  
direct=1
rw=write

```

### 128KB with 1 read (running alone)
```
[global]
directory= [ location of the sdb ]
bs=128k
numjobs = 1
group_reporting = 1  
direct=1
rw=read

```

### 128KB with 1 write (running alone)
```
[global]
directory= [ location of the sdb ]
bs=128k
numjobs = 1
group_reporting = 1  
direct=1
rw=write

```



### 4KB with 1 read and 1 write
- filename: 1_1_4_read_write.fio

```
[global]
directory= [ location of the sdb ]
group_reporting = 1  
direct=1

[reader]
bs=4k
numjobs = 1
rw=seqread / randread

[writer]
bs=4k
numjobs = 1
rw=seqwrite / randwrite

```

### 4KB with 16 read and 16 write
- filename: 16_16_4_4_read_write.fio

```
[global]
directory= [ location of the sdb ]
iodepth=2
numjobs = 16
group_reporting = 1  
direct=1

[reader]
bs=4k
rw=seqread / randread

[writer]
bs=4k
rw=seqwrite / randwrite

```

### 4KB of 16 read and 128KB of 16 read
- filename: 16_16_4_128_read_write.fio

```
[global]
directory= [ location of the sdb ]
iodepth=2
numjobs = 16
group_reporting = 1  
direct=1

[4k_reader]
bs=4k
rw=seqread / randread

[128k_reader]
bs=128k
rw=seqread / randread

```

### 4KB of 16 write and 128KB of 16 write
- filename: 16_16_4_128_read_write.fio

```
[global]
directory= [ location of the sdb ]
iodepth=2
numjobs = 16
group_reporting = 1  
direct=1

[4k_writer]
bs=4k
rw=seqwrite / randwrite

[128k_writer]
bs=128k
rw=seqwrite / randwrite

```



# 2_ Evaluation of Task fairness
- use proportinoal slowdown
- When n tasks compete for I/O simultaneously, equal resource sharing suggest that each task should experience a factor of n slowdown compared to running alone.
- When all tasks experience better performance than the proportional slowdown, we further measure fairness according to the slowdown of the slowest tasks.
- Specifically, scheduler S1 achieves better fairness than scheduler S2 if the slowest task under S1 makes more progress than the slowest task under S2.

### implementation
- ( avg of I/O of concurrent tasks / I/O of one task running alone )


# 3_ Evaluation of Responsiveness

### implementation
- ( avg of worst case (99.9 percentile) I/O request )


# 4 the ratio of random I/O latency over sequential I/O latency.
- check whether this is necessary or not.
- because it is mostly used for testing Spatial Proximity
