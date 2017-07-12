## 1 Depth Performance Evaluation
- number of depth [1, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64]
- number of io-depth (within 32)[1 - 20] -> check first whether there is a difference


#### 32 read with 4KB
- filename: 1_4_read.fio

```
[read_one_4]
filename=/dev/sdb
bs=4k
numjobs=32
direct=1
rw=read
runtime=20

```

#### 32 write with 4KB
- filename: 1_4_write.fio

```
[write_one_4]
filename=/dev/sdb
bs=4k
numjobs=1
direct=1
rw=write
runtime=20

```

#### 1 read (running alone) with 128KB
- filename: 1_128_read.fio

```
[read_one_128]
filename=/dev/sdb
bs=128k
numjobs=1
direct=1
rw=read
runtime=20

```

#### 1 write (running alone) with 128KB
- filename: 1_128_write.fio

```
[write_one_128]
filename=/dev/sdb
bs=128k
numjobs=1
direct=1
rw=write
runtime=20

```



#### 1 read and 1 write with 4KB
- filename: 1_1_4_read_write.fio

```
[global]
filename=/dev/sdb
runtime=20
direct=1

[reader_4]
bs=4k
numjobs=1
rw=read

[writer_4]
bs=4k
numjobs=1
rw=write

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
