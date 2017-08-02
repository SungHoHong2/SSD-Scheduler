### NOOP FIO test results


#### FIO operation used in testing
- test run by using fio files
- test run by using direct fio parameters

```
[job_example]
filename=/dev/sdd
direct=1
rw=read
bs=4k
numjobs= 1 ~ 64
rw=read
runtime=20            
```

```
fio --filename=/dev/sdd --name=noop_numjob_test --rw=read --bs=4k --numjobs= 1 ~ 64 --runtime=20 --direct=1
```

#### Test results of raw data
1. [test run with 1 job](jobs_1)
2. [test run with 4 job](jobs_4)
3. [test run with 8 job](jobs_8)
4. [test run with 16 job](jobs_16)
5. [test run with 32 job](jobs_32)
6. [test run with 64 job](jobs_64)


#### Test results with spread_sheet
- ran the same test for 3 times
- standard deviation added to the result
- y-axis represent the total I/O throughput
- x-axis represent the number of jobs
- [google excel sheet link](https://docs.google.com/spreadsheets/d/1FU2c1kCZQ9wPeN_RGyVjkpLM9sAXoHOXBHTeeDjyFQo/edit?usp=sharing)

![First Image](test.png)
