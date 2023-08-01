FROM arm64v8/ubuntu:20.04

RUN apt update && \
    apt install -y libssl-dev locales && \
    rm -rf /var/lib/apt/lists/*
RUN sed -i 's/# en_US.UTF-8/en_US.UTF-8/g' /etc/locale.gen
RUN sed -i 's/# zh_CN.GBK/zh_CN.GBK/g' /etc/locale.gen
RUN locale-gen

COPY data/jpeg-preview.json /home/templates/
COPY data/config/modbus_cfg.json /home/config/
COPY data/config/NotoSansCJK-Regular.ttc /home/config/
COPY release/lib/ /usr/local/lib/
COPY release/bin/ /usr/local/bin/

ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

WORKDIR /home

ENTRYPOINT [ "node_srv" ]