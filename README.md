
## 操作步骤

1. 编译整个工程，在顶层目录下执行 `make` 命令即可；

2. 编译结束后会在顶层目录下生成一个 output 目录，里面保存了后续需要用到的文件；

3. 测试时进入 output 目录 (建议以root用户执行)：

    1. 加载自定义的 NVMe 驱动模块，执行 `./insmod.sh` 脚本；

    2. 运行 NVMe 测试程序，eg: `./nvmetool /dev/nvme0` (PS: NVMe 设备节点名称需要根据实际情况来配置)

### 查看版本信息

在成功加载 dnvme.ko 模块后，执行 `cat /sys/module/dnvme/version` 命令，即可查看该模块当前的版本信息。
