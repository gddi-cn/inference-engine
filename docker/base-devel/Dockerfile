FROM nvidia/cuda:11.4.0-cudnn8-devel-ubuntu20.04

RUN sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y git pkgconf ninja-build ccache libssl-dev wget && \
    rm -rf /var/lib/apt/lists/*

# CMake
RUN cd /tmp && \
    wget https://github.com/Kitware/CMake/releases/download/v3.24.0/cmake-3.24.0-linux-x86_64.sh && \
    chmod +x cmake-3.24.0-linux-x86_64.sh && \
    ./cmake-3.24.0-linux-x86_64.sh --prefix=/usr/local --exclude-subdir --skip-license && \
    rm -rf /tmp/*

# Armv8 交叉编译链
RUN apt update && \
    apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu gdb-multiarch && \
    rm -rf /var/lib/apt/lists/*

RUN cd /tmp && \
    git clone -b 4.5.5 --depth=1 http://git.mirror.gddi.io/mirror/opencv.git && cd opencv && \
    cmake -S . -Bbuild -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local/aarch64-tx5368-soc \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_PROCESSOR=armv8 -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
        -DBUILD_LIST=core,highgui,improc,imgcodecs,video,videoio,photo,gapi,calib3d && \
    cmake --build build/ --target install && \
    rm -rf /tmp/*

# Conan 包管理工具
RUN ls -al / && \
    apt update && \
    apt install -y python3 python3-pip && \
    pip3 install conan==1.57.0 && \
    rm -rf /var/lib/apt/lists/*

# Docker
RUN apt update && \
    apt install -y curl lsb-release && \
    rm -rf /var/lib/apt/lists/*
RUN mkdir -p /etc/apt/keyrings && \
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
        $(lsb_release -cs) stable" | tee /etc/apt/sources.list.d/docker.list > /dev/null && \
    apt update && \
    apt -y install docker-ce docker-ce-cli containerd.io docker-compose-plugin && \
    rm -rf /var/lib/apt/lists/*

# User && ZSH
RUN apt update && \
    apt install -y sudo zsh && \
    rm -rf /var/lib/apt/lists/*

ARG uid=1000
ARG gid=1000
RUN groupadd -r -f -g ${gid} gddi && useradd -o -r -l -u ${uid} -g ${gid} -ms /bin/bash gddi && \
    usermod -aG sudo gddi && \
    echo 'gddi:gddi1234' | chpasswd && \
    mkdir -p /workspace && chown gddi /workspace && \
    gpasswd -a gddi docker

# Clear
RUN apt clean autoclean && apt autoremove --yes

# Env
ENV PATH=/usr/local/cuda-11.4/bin:/usr/local/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/local/cuda-11.4/lib64:/usr/local/lib:$LD_LIBRARY_PATH
ENV C_INCLUDE_PATH=/usr/local/include:$C_INCLUDE_PATH
ENV CPLUS_INCLUDE_PATH=/usr/local/include:$CPLUS_INCLUDE_PATH

USER gddi

RUN conan remote add gddi-conan-local http://devops.io:8081/artifactory/api/conan/gddi-conan-local --insert 0 && \
    conan user -p Gddi@1234 -r gddi-conan-local gddi


RUN git clone https://github.com/ohmyzsh/ohmyzsh.git ~/.oh-my-zsh && \
    git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions && \
    git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting && \
    git clone https://github.com/zsh-users/zsh-history-substring-search ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-history-substring-search && \
    cp ~/.oh-my-zsh/templates/zshrc.zsh-template ~/.zshrc && \
    sed -i '/^#\ ZSH_CUSTOM/c ZSH_CUSTOM=~\/.oh-my-zsh\/custom' ~/.zshrc && \
    sed -i '/^plugins/c plugins=(git zsh-autosuggestions zsh-syntax-highlighting zsh-history-substring-search)' ~/.zshrc

WORKDIR /workspace

RUN ["/bin/zsh"]

# docker build . -f docker/base-devel/Dockerfile -t hub.gddi.com/devel/inference-engine-devel:1.0.1