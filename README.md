# BlockSci

This is a fork of the original BlockSci repository with extensions for the analysis of CoinJoin transactions.
This fork is a part of the Master's thesis developed for CRoCS laboratory at Masaryk University, Brno, Czech Republic.

## Quickstart
This version of BlockSci is intended to be run in Docker. As of 2024, BlockSci runs on Python 3.7 with many outdated libraries. If you **really** want to run it on your machine, you can try to follow the installation
instructions from the original repository, however, unless you are running Ubuntu 20.04, you will most likely encounter issues.

BlockSci is a high-performance tool for blockchain science and exploration. It consists of two main components: a C++ library that performs high-performance blockchain analysis, and a Python library to provide high-level access to the results.
For the Python library, which is the main interface for the user, we set up Jupyter notebooks with examples of how to use BlockSci for various analyses.

To run BlockSci in Docker, the following steps are required:

1. Clone this repository
    1. You also need to clone with submodules. For that, either use `--recurse-submodules` while cloning, or, in the cloned repository, follow with `git submodule init` and `git submodule update --recursive`.
2. Install [Docker](https://docs.docker.com/get-docker/)
3. Build the Docker image by running `docker build --build-arg NTHREADS=<number of threads> -t blocksci-cj .` in the root of the repository. This is just an initial setup to get all the libraries, and the *real* compilation will happen later.
    1. We use `uv` to speed up the `blockscipy` installation, since it takes a long time.
    2. Use NTHREADS build argument to control how many threads are used (passed to `make -j${NTHREADS}`).

Now - since BlockSci is a memory-intensive and disk-consuming tool, we **strongly** recommend you to not hold the BlockSci data directly in the image, but to mount a persistent volume to the container.

An example way to set up the mounts is as follows:

- in `/mnt/blocksci` we mount this repository to be able to develop and write code without rebuilding the image.
- in `/mnt/data` we mount the blockchain directory (e.g. `~/.bitcoin`). This folder is used just for the initial parsing and does not need to be a persistent volume, since it's just the full node folder.
- in `/mnt/anal` we mount the volume where the BlockSci data will be stored. This is the most important volume, as it will contain the parsed blockchain data and the results of the analyses. Be careful as this volume can grow quite large.

To run the container with the mounted volumes, you can use the following command:

```bash
docker run --name blocksci_container --replace -p <notebook port>:8888 -v <this repository folder>:/mnt/blocksci -v <bitcoin fullnode directory>:/mnt/data  -v <analysis volume>:/mnt/anal -it --entrypoint /bin/bash blocksci-cj:latest
```

The `--replace` flag is used to remove the container with the same name if it already exists. The `-p` flag is used to expose the Jupyter notebook on the specified port. The `-v` flag is used to mount the volumes. The `-it` flag is used to run the container in interactive mode. The `--entrypoint /bin/bash` flag is used to run the container in bash mode, so you can run the Jupyter notebook manually.

Now, as we have everything mounted and we are connected to the container, we can run the "classic" BlockSci setup.

1. First, `cd /mnt/blocksci`.
2. Run `./build.sh`
    1. This rebuilds the application with the correct development settings, correct filepaths etc.
    2. This also runs the Notebooks, so when they are started, just turn them off, as we have no parsed data yet.
2. Parse blockchain 
    1. `blocksci_parser <config file> generate-config bitcoin <blocksci data directory> --disk <fullnode data directory>`
        1. here, `<config file>` is a file that will be created, for example `/mnt/anal/blocksci_config.json`
        2. `<blocksci data directory>` is `/mnt/anal/blocksci_data` if you follow our example with the mounted volume 
        3. `<fullnode data directory>` is `/mnt/data`
    2. That just creates the configuration. To actually parse the blockchain, run `blocksci_parser <config file> update` 
    3. Now just wait for a while and everything should be parsed. We suggest for the **initial** run to have the blockchain directory on some fast read disk, as it takes a while.
3. After everything smoothly parses, run `./build.sh` once again. Now, there should be a Jupyter notebook running at port `<notebook port>`.
4. For smoother learning experience we suggest to read the docs below, as well as turning Hinterland on.
    1. [Hinterland](https://jupyter-contrib-nbextensions.readthedocs.io/en/latest/nbextensions/hinterland/README.html) is a jupyter extension that enables autocomplete in the notebook.

## Documentation from the original repository

We provide instructions in our [online documentation](https://citp.github.io/BlockSci/):

- [Installation instructions](https://citp.github.io/BlockSci/setup.html)
- [Using BlockSci](https://citp.github.io/BlockSci/using-blocksci.html)
- [Guide for the fluent interface](https://citp.github.io/BlockSci/fluent-interface.html)
- [Module reference for the Python interface](https://citp.github.io/BlockSci/reference/reference.html)
- [Troubleshooting](https://citp.github.io/BlockSci/troubleshooting.html)

Our [FAQ](https://github.com/citp/BlockSci/wiki) contains additional useful examples and tips.


