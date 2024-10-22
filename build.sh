#!/bin/bash

cd /mnt/blocksci-compilable

mkdir -p build && \
    cd build && \
    CC=gcc-7 CXX=g++-7 cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j258 && \
    make install

cd /mnt/blocksci-compilable

pip3 install jupyter notebook
pip3 install jupyter_contrib_nbextensions
jupyter contrib nbextension install --user


CC=gcc-7 CXX=g++-7 pip3 install -e blockscipy

source .venv/bin/activate

cd Notebooks

jupyter notebook --ip=0.0.0.0 --allow-root

