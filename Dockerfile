# Use a multi-architecture base image
ARG TARGETARCH
FROM --platform=linux/${TARGETARCH} opensuse/leap:15.4

# Install necessary dependencies and utilities
RUN zypper refresh && \
    zypper install -y \
    bash \
    git \
    make \
    gcc \
    gcc-c++ \
    cmake \
    gdb \
    pkg-config \
    glib2-devel \
    iputils \
    net-tools \
    curl \
    wget \
    vim \
    && zypper clean --all

RUN zypper refresh
RUN zypper install -y gcc13 gcc13-c++

ENV CXXFLAGS="-w -fpermissive"

#RUN zypper install -y ndctl libndctl-devel pandoc tar gzip
RUN zypper install -y pandoc tar gzip

# install boost
RUN wget https://archives.boost.io/release/1.65.0/source/boost_1_65_0.tar.gz 
RUN tar -xzf boost_1_65_0.tar.gz  && cd boost_1_65_0 && ./bootstrap.sh --with-libraries=log,filesystem,program_options && ./b2 install --with-context --with-log --with-filesystem  --with-program_options && cd /

# install yaml-cpp
RUN git clone https://github.com/jbeder/yaml-cpp.git
RUN cd yaml-cpp && mkdir build && cd build && cmake ..  -Wno-dev && make && make install

# install pmdk
RUN git clone https://github.com/pmem/pmdk.git pmdk
RUN cd pmdk && make NDCTL_ENABLE=n PMEMOBJ_IGNORE_DIRTY_SHUTDOWN=y PMEMOBJ_IGNORE_BAD_BLOCKS=y install 


#install nvmm-openfam
RUN export ARCH=$(uname -m) && if [ "$ARCH" = "amd64" ] || [ "$ARCH" = "x86_64" ]; then \
    mkdir cmake && cd cmake && wget https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.tar.gz && \
    tar xzf cmake-3.30.5-linux-x86_64.tar.gz && mv cmake-3.30.5-linux-x86_64 /opt/cmake && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake; fi


RUN export ARCH=$(uname -m) && if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "aarch64" ]; then\
    mkdir cmake && cd cmake && wget https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-aarch64.tar.gz && \
    tar xzf cmake-3.30.5-linux-aarch64.tar.gz && mv cmake-3.30.5-linux-aarch64 /opt/cmake && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake; fi

RUN cd / && git clone https://github.com/opencube-horizon/openfam-nvmm.git nvmm && cd nvmm && \ 
    git checkout feature/arm-support && mkdir build && cd build && cmake ..  -Wno-dev -DFAME=OFF -DCMAKE_BUILD_TYPE=Debug && make && make install

# install radixtree
RUN git clone https://github.com/opencube-horizon/openfam-radixtree.git radixtree
RUN cd radixtree/ && git checkout bugfix/build-system
COPY opencube-openfam/_hotfixes.patch .
RUN cd radixtree/ && git apply ../_hotfixes.patch
RUN cd radixtree/ && mkdir build && cd build && cmake -Wno-dev .. -DMETRICS=OFF -DFAME=OFF -DCMAKE_BUILD_TYPE=Debug && make radixtree
RUN cp /radixtree/build/src/libradixtree.so /usr/lib64/
ENV CMAKE_PREFIX_PATH=/radixtree/build/:${CMAKE_PREFIX_PATH}


#install pmxi
RUN zypper install -y autoconf libtool libevent-devel libhwloc-devel libfabric-devel openmpi2
RUN wget https://github.com/openpmix/openpmix/releases/download/v4.1.1/pmix-4.1.1.tar.gz
RUN tar -xzf pmix-4.1.1.tar.gz  &&\
    cd pmix-4.1.1 && ./autogen.pl &&\
    mkdir build && cd build &&\
    ../configure && make all install

ENV LD_LIBRARY_PATH=/usr/local/lib/pmix/:${LD_LIBRARY_PATH}


#install grpc
RUN zypper install -y -y libopenssl-devel
RUN git clone -b v1.39.0 https://github.com/grpc/grpc --recursive grpc
RUN cd grpc && mkdir -p cmake/build && cd cmake/build && cmake ../..  -Wno-dev -DBUILD_SHARED_LIBS=ON -DgRPC_INSTALL=ON -DABSL_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Debug -DgRPC_BUILD_TESTS=OFF  -D_GLIBCXX_USE_CXX11_ABI=0 && make && make install
ENV CMAKE_PREFIX_PATH=/usr/local/lib/grpc/:${CMAKE_PREFIX_PATH}
ENV LD_LIBRARY_PATH=/usr/local/lib/:${LD_LIBRARY_PATH}
ENV LD_LIBRARY_PATH=/usr/local/lib64/:${LD_LIBRARY_PATH}


RUN zypper install -y python311 python311-pip
RUN pip3 install ruamel_yaml
RUN pip3 install tabulate



RUN git clone https://github.com/opencube-horizon/OpenFAM.git opencube-openfam && cd opencube-openfam && git checkout opencube-stable-release 
RUN cd opencube-openfam/ && mkdir build && cd build && cmake .. -DFAME=OFF  -DCMAKE_CXX_FLAGS="-pthread" -DCMAKE_BUILD_TYPE=Debug -D_GLIBCXX_USE_CXX11_ABI=0 -DWITH_PMIX=1 -DWITH_PMI2=0 -Wno-dev && make && make install



RUN cp /opencube-openfam/tools/openfam_adm.py  /usr/local/bin/openfam_adm
RUN echo -e "--model=memory_server\n\
--cisinterface=rpc\n--memserverinterface=rpc\n\
--metaserverinterface=rpc\n\
--rpc_framework=grpc\n\
--cisserver={rpc_interface:127.0.0.1,rpc_port:8787}\n\
--memservers=0:{memory_type:volatile,fam_path:/dev/shm/vol/,rpc_interface:127.0.0.1,rpc_port:8793,libfabric_port:7500,if_device:eth0}\n\
--metaservers=0:{rpc_interface:127.0.0.1,rpc_port:8788}" > args.txt

ENV USER=root
RUN openfam_adm --create_config_files --install_path=/opencube-openfam/ --config_file_path=/usr/local/ @args.txt
ENV OPENFAM_INSTALL_DIR=/usr/local/
ENV OPENFAM_ROOT=/usr/local/