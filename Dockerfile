# Use Ubuntu 20.04 LTS as the base image
FROM ubuntu:20.04

# Avoid prompts from apt-get
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y software-properties-common
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y && apt-get update
RUN apt-get install -y cmake libtool autoconf libboost-filesystem-dev \
    libboost-iostreams-dev libboost-serialization-dev libboost-thread-dev \
    libboost-test-dev libssl-dev libjsoncpp-dev libcurl4-openssl-dev \
    libjsoncpp-dev libjsonrpccpp-dev libsnappy-dev zlib1g-dev libbz2-dev \
    liblz4-dev libzstd-dev libjemalloc-dev libsparsehash-dev python3-dev \
    python3-pip pkg-config git g++-7 gcc-7 ffmpeg libcairo2 libcairo2-dev curl


ADD . /blocksci-compilable

# Clone BlockSci repository
# RUN git clone https://github.com/mmstanone/blocksci-compilable.git

RUN curl -LsSf https://astral.sh/uv/install.sh | sh
RUN /root/.cargo/bin/uv python install 3.8.20
RUN /root/.cargo/bin/uv python pin 3.8.20

RUN /root/.cargo/bin/uv run which pip3

RUN mkdir -p /usr/lib/python3.8/site-packages/

RUN cd /blocksci-compilable && \
    /root/.cargo/bin/uv venv && CC=gcc-7 CXX=g++ /root/.cargo/bin/uv run pip3 install -r /blocksci-compilable/pip-all-requirements.txt

# Build BlockSci
RUN cd blocksci-compilable && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    CC=gcc-7 CXX=g++-7 cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j258 && \
    make install


# Install BlockSci Python bindings

RUN cd blocksci-compilable && rm -rf blockscipy/build && \
    /root/.cargo/bin/uv venv && CC=gcc-7 CXX=g++-7 /root/.cargo/bin/uv run pip3 install -e blockscipy

# remove the build folder for blockscipy, as we will rebuild again anyway

RUN rm -rf /blocksci-compilable/blokscipy/build

# Set the default command for the container
CMD ["/bin/bash"]

