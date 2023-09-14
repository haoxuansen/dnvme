

### 一键编译&运行

&emsp;&emsp;为了避免在编译运行中出现的权限问题，建议先切换成 root 用户（eg：`su root`）。随后在工程顶层目录下运行 zoo 脚本（eg： `./zoo`）即可。

&emsp;&emsp;zoo 脚本会自动编译工程、加载驱动模块以及运行 nvmetool APP 程序。nvmetool 默认会打开设备节点 /dev/nvme0，当存在多个 nvme 设备时，可以通过传参的方式来指定设备节点名称（eg：`./zoo devnode=/dev/nvme2`）。

### 工程配置

&emsp;&emsp;用户可以根据需要调整工程的默认配置，如：编译哪些模块、开启 debug 调试……执行 `make menuconfig ` 命令后可进入配置修改界面。

### 工程编译

&emsp;&emsp;用户也可以根据需要编译指定目录及子目录的文件。注意：当不同目录存在依赖关系时，需要根据依赖关系的先后顺序进行编译，否则会出现报错。

- 编译整个工程：在顶层目录下执行 `make` 命令即可

- 编译子模块：在子目录下执行 `make` 命令即可

	编译生成的成果物会拷贝到顶层目录的 release 文件夹中，编译结束后，可以切换到该目录下查看产物。

### 工程清理

- 清理中间产物：执行 `make clean` 命令即可
- 清理所有产物：执行 `make distclean` 命令即可

### 工程运行

&emsp;&emsp;编译结束后，切换到 release 目录下：

1. 加载自定义的 NVMe 驱动模块，执行 `./insmod.sh` 脚本；

2. 运行 NVMe 测试程序，eg: `./nvmetool /dev/nvme0` (PS: NVMe 设备节点名称需要根据实际情况来配置)

### 查看版本信息

&emsp;&emsp;在成功加载 dnvme.ko 模块后，执行 `cat /sys/module/dnvme/version` 命令，即可查看该模块当前的版本信息。

### 扩展软件包

```
pip install sphinx

pip install sphinx-design
pip install sphinx-tabs
pip install sphinx-togglebutton

pip install sphinx-rtd-theme
```
