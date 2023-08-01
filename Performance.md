# 不同体系下性能对比

> 以下是对比`BlockingConcurrentQueue`与`cppzmq`的效率对比
> 在pub&sub模式下，sub订阅效率因为要过滤，所以巨差。

```
sock_url: inproc://benchmark
last_endpoint : inproc://benchmark
Pub is ready! : inproc://benchmark
Sub connected!
producer total: 1,000,000
producer time: 146,118us, 6,843,783 msg/s
consumer time: 1,479,251us, 676,017 msg/s

bench for blocking,   256, 976.6K Sample
producer total: 1,000,000
producer time: 189,193us, 5,285,607 msg/s
consumer time: 189,234us, 5,284,462 msg/s
POOL size: 0
```