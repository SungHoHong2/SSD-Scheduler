
#### run the fio until I/O commits 1 gigabyte

```
[read_one_4]
filename=/dev/sdb
bs=4k
numjobs=4
direct=1
rw=read
size=1g
```
