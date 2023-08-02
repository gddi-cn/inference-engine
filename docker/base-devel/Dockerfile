FROM ubuntu:20.04

RUN sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y git pkgconf ninja-build libssl-dev wget build-essential autoconf libtool && \
    rm -rf /var/lib/apt/lists/*

# CMake
RUN cd /tmp && \
    wget https://github.com/Kitware/CMake/releases/download/v3.24.0/cmake-3.24.0-linux-x86_64.sh && \
    chmod +x cmake-3.24.0-linux-x86_64.sh && \
    ./cmake-3.24.0-linux-x86_64.sh --prefix=/usr/local --exclude-subdir --skip-license && \
    rm -rf /tmp/*

# Armv8 交叉编译链
RUN apt update && \
    apt install -y gcc-10-aarch64-linux-gnu g++-10-aarch64-linux-gnu gdb-multiarch && \
    rm -rf /var/lib/apt/lists/*

RUN cd /tmp && \
    wget https://s3.local.cdn.desauto.net/apptpl/other/aarch64-tx5368-soc.tar.gz && \
    tar -zxvf aarch64-tx5368-soc.tar.gz -C /usr/local

# User && ZSH
RUN apt update && \
    apt install -y sudo zsh && \
    rm -rf /var/lib/apt/lists/*

ARG uid=1000
ARG gid=1000
RUN groupadd -r -f -g ${gid} ubuntu && useradd -o -r -l -u ${uid} -g ${gid} -ms /bin/bash ubuntu && \
    usermod -aG sudo ubuntu && \
    echo 'ubuntu:ubuntu' | chpasswd && \
    mkdir -p /workspace && chown ubuntu /workspace

# Clear
RUN apt clean autoclean && apt autoremove --yes

# Env
ENV C_INCLUDE_PATH=/usr/local/include:$C_INCLUDE_PATH
ENV CPLUS_INCLUDE_PATH=/usr/local/include:$CPLUS_INCLUDE_PATH

USER ubuntu

RUN git clone https://github.com/ohmyzsh/ohmyzsh.git ~/.oh-my-zsh && \
    git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions && \
    git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting && \
    git clone https://github.com/zsh-users/zsh-history-substring-search ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-history-substring-search && \
    cp ~/.oh-my-zsh/templates/zshrc.zsh-template ~/.zshrc && \
    sed -i '/^#\ ZSH_CUSTOM/c ZSH_CUSTOM=~\/.oh-my-zsh\/custom' ~/.zshrc && \
    sed -i '/^plugins/c plugins=(git zsh-autosuggestions zsh-syntax-highlighting zsh-history-substring-search)' ~/.zshrc

WORKDIR /workspace

RUN ["/bin/zsh"]

# docker build . -f docker/base-devel/Dockerfile -t hub.gddi.com/devel/inference-engine-tx5368:1.0.0