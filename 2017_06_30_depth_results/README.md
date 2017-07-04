## I/O benchmarks for evaluating the Depth Performance
- evaluate the results based on different depth
  - depths: [ 1, 4, 8, 16, 32, 64 ]

#### 32 read (running alone) with 4KB
- filename: 32_4_read.fio

```
[read_one_4]
filename=/dev/sdb
bs=4k
numjobs=32
direct=1
rw=read
runtime=20

```

#### 32 write (running alone) with 4KB
- filename: 32_4_write.fio

```
[write_one_4]
filename=/dev/sdb
bs=4k
numjobs=32
direct=1
rw=write
runtime=20

```

#### 32 read (running alone) with 128KB
- filename: 32_128_read.fio

```
[read_one_128]
filename=/dev/sdb
bs=128k
numjobs=32
direct=1
rw=read
runtime=20

```

#### 32 write (running alone) with 128KB
- filename: 32_128_write.fio

```
[write_one_128]
filename=/dev/sdb
bs=128k
numjobs=32
direct=1
rw=write
runtime=20

```


#### 16 read and 16 write with 4KB
- filename: 16_4_16_4_read_write.fio

```
[global]
filename=/dev/sdb
runtime=20
direct=1

[reader_4]
bs=4k
numjobs=16
rw=read

[writer_4]
bs=4k
numjobs=16
rw=write

```

#### 16 read of 4KB and 16 read of 128KB  
- filename: 16_4_16_128_read.fio

```
[global]
filename=/dev/sdb
runtime=20
direct=1

[reader_4]
bs=4k
numjobs=16
rw=read

[reader_128]
bs=128k
numjobs=16
rw=read

```

#### 16 write of 4KB and 16 write of 128KB
- filename: 16_4_16_128_write.fio

```
[global]
filename=/dev/sdb
runtime=20
direct=1

[writer_4]
bs=4k
numjobs=16
rw=write

[writer_128]
bs=128k
numjobs=16
rw=write

```



## 2_ Evaluation of Task fairness
- use proportinoal slowdown
- When n tasks compete for I/O simultaneously, equal resource sharing suggest that each task should experience a factor of n slowdown compared to running alone.
- When all tasks experience better performance than the proportional slowdown, we further measure fairness according to the slowdown of the slowest tasks.
- Specifically, scheduler S1 achieves better fairness than scheduler S2 if the slowest task under S1 makes more progress than the slowest task under S2.

```
implementation
- ( avg of I/O of concurrent tasks / I/O of one task running alone )
- ( I/O performance of the slowest task / I/O of one task running alone )
```


## 3_ Evaluation of Responsiveness
```
implementation
- ( avg of worst case (99.9 percentile) I/O request )
```
