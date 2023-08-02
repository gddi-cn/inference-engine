# 1. How To Build

## 1.1 Linux
### 1.1.1 基础开发环境
```bash
# 创建容器
docker run -itd --name=cpp-devel --privileged=true --net=host gddi2020.qicp.vip:12580/devel/inference-engine-tx5368:1.0.0
```

### 1.1.2 CMake 编译
```bash
# Nvidia / Intel
cmake -S . -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Release

# 交叉编译
cmake -S . -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64_linux_tx5368.toolchain
cmake --build build
```

### 1.2.2 VSCode(CMake Tools)
> `Ctrl + Shift + P` 打开配置，搜索设置 `cmake.generator`，设置 `Ninja` 这样可以提高编译速度。

# 2. 配置文件说明

```typescript
// 以下为typescript代码
interface IConfig {
    version: 'v0';                              // 固定为'v0', 正式Release的时候会固定下来。
    nodes: {
        id: number;                             // 节点ID
        type: string;                           // 节点类型，必须是程序支持的类型
        name: string;                           // 节点名，可以任意命名
        runner: string;                         // 节点运行的目标线程名（目标线程会自动创建）
        props: {                                // 节点属性，可选，key:value的一个字典，用于配置节点
            [index: string]: string | number
        }
    }[];
    pipe: [number, number, number, number][]    // 节点数据流，from[ep_out], to[ep_in] 四个参数
}
```

# 2. FFmpeg 编译

# 3. 后处理 SDK 接口说明
## 编译环境
- ubuntu18.04
- gcc-version: 7.5
- cmake: 3.24.0
- pkg-config: 0.29.1

## 安装依赖
```bash
apt install -y cmake

# 交叉编译
apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

## 重新配置 pkg-config prefix 路径
```
sed -i "1s/.*/prefix=$(echo $PWD | sed -e 's/\//\\\//g')/g" lib/pkgconfig/gddi_post.pc
sed -i "1s/.*/prefix=$(echo $PWD | sed -e 's/\//\\\//g')/g" lib/pkgconfig/gddi_codec.pc
```

## 编译测试
```bash
# 配置
cmake -S . -G Ninja -Bbuild

# 编译
cmake --build buid/
```

## 模块说明
- [编解码模块](doc/codec.md)
- [跟踪模块](doc/target_tracker.md)
- [越界计数模块](doc/cross_border.md)

# 4. 打包发布
```bash
docker build . -f docker/xxxxxx/Dockerfile -t devops.io:12580/inference/xxxxxx-engine:v1.4.27-fix{MM}{dd}
```