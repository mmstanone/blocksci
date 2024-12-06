#!/bin/bash

cd /mnt/blocksci

(mkdir -p build && \
    cd build && \
    CC=gcc-7 CXX=g++-7 cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j258 && \
    make install) || exit 1

cd /mnt/blocksci

pip3 install jupyter notebook
pip3 install jupyter_contrib_nbextensions
jupyter contrib nbextension install --user


CC=gcc-7 CXX=g++-7 pip3 install -e blockscipy || exit 1

cd Notebooks

jupyter notebook --ip="0.0.0.0" --allow-root || exit 1

