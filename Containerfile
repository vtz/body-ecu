FROM ubuntu:22.04

ARG ZEPHYR_SDK_VERSION=0.17.0

ENV DEBIAN_FRONTEND=noninteractive
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}
ENV PATH="/root/.local/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
        wget xz-utils file make gcc g++ \
        python3-dev python3-pip python3-venv python3-setuptools \
        python3-wheel python3-tk libsdl2-dev libmagic1 \
    && rm -rf /var/lib/apt/lists/*

RUN ARCH=$(uname -m) \
    && wget -q "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-${ARCH}_minimal.tar.xz" \
    && tar xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-${ARCH}_minimal.tar.xz -C /opt \
    && rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-${ARCH}_minimal.tar.xz \
    && ${ZEPHYR_SDK_INSTALL_DIR}/setup.sh -t arm-zephyr-eabi -h -c

RUN pip3 install --no-cache-dir \
        west pyelftools pyyaml pykwalify packaging intelhex docutils

WORKDIR /workdir
COPY scripts/container-entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["/bin/bash"]
