该 Project 是基于 [dpdk-switch](https://github.com/dfshan/dpdk-switch.git) 上修改

数据包流向：receive.c -> forwarding.c -> output_queue.c -> transmit.c

----

##### 文件夹

- app：该文件夹里是不同 schemes
- sampe-code：采样代码
- dpdk-stable-17.08.1：dpdk官方的源码

---

##### 编译

1. 首先，需要导入环境变量

   ```shell
   export RTE_SDK=/path/to/dpdk-source-dir	# 填 dpdk 源码文件位置
   export RTE_TARGET=x86_64-native-linuxapp-gcc	# 填写机器编译出来版本
   ```

2. 编译
   
   ```shell
   make
   ```

   
   
3. 运行
   
   ```shell
   ./build/app/main -c 0xf --log-level=7 -- -p 0xf		# 也可以直接执行 ./runit.sh
   ```
   
   这里可以看出需要 4个lcores 和 4个ports，若机子配置不满足或者需要更好性能，不仅需要改执行命令也要改source。

4. References

"Optimizing Flow Completion Time via Adaptive Buffer Management in Data Center Networks ", International Conference on Parallel Processing (ICPP) 2021
