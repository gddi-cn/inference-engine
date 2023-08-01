# 在原生环境下编译


> * **Ubuntu 22.04 LTS**
>   1. 安装以下依赖   
>      `sudo apt install cmake build-essential libboost-dev libboost-program-options-dev libboost-filesystem-dev`   
>      `sudo apt install libssl-dev libcurl4-openssl-dev libopencv-dev`
>      `sudo apt install libavcodec-dev libavutil-dev libavformat-dev libavdevice-dev`
> 
> 
> 

## 

* spdlog
```shell
git clone http://git.mirror.gddi.io/mirror/spdlog.git
cd spdlog
git checkout -b v1.10.0 v1.10.0
mkdir build && cd build
cmake .. && make -j
sudo make install
```


* libhv
```shell
git clone http://git.mirror.gddi.io/mirror/libhv.git
cd libhv && git checkout -b v1.2.5 v1.2.5
mkdir build && cd build
cmake .. && make -j
sudo make install 
```

* eigen
```shell
git clone http://git.mirror.gddi.io/mirror/eigen.git
cd eigen && git checkout -b 3.4.0 3.4.0
mkdir build && cd build
cmake .. && make -j
sudo make install
```

* blend2d
```shell
git clone http://git.mirror.gddi.io/mirror/blend2d.git
git clone http://git.mirror.gddi.io/mirror/asmjit.git
mkdir blend2d/build && cd blend2d/build
cmake .. && make -j
sudo make install
```