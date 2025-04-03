FROM ubuntu:24.04

LABEL maintainer="lklic"
ENV TZ=Europe/Rome
# Set environment variables for memory allocator optimizations
ENV MIMALLOC_LARGE_OS_PAGES=1
ENV MALLOC_CONF="thp:always,metadata_thp:always"
ENV GLIBC_TUNABLES=glibc.malloc.hugetlb=1
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
ENV CURL_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt-get update \
  && apt-get install -y curl wget libcurl4-openssl-dev libopencv-dev libmicrohttpd-dev \
     libjsoncpp-dev cmake git build-essential clang lld libmimalloc-dev ca-certificates \
     libboost-all-dev libtbb-dev
     
# Create app directory
RUN mkdir -p /app
WORKDIR /app

# Selective copy of source files (matching the Nix Dockerfile approach)
COPY CMakeLists.txt ./
COPY cmake ./cmake/
COPY include ./include/
COPY python ./python/
COPY src ./src/
COPY examples ./examples/
COPY visualWordsORB.dat ./

# Create build and data directories
RUN mkdir -p build data
WORKDIR /app/build

# Use clang as the compiler with optimized flags matching Nix configuration
RUN cmake ../ \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="-mavx512f -mavx512dq -mavx512fp16 -mavx512bf16" \
    -DCMAKE_CXX_FLAGS="-std=c++17 -mavx512f -mavx512dq -mavx512fp16 -mavx512bf16" \
    -DSIMSIMD_TARGET_SAPPHIRE=1 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PASTEC_LIB=ON \
    -DBUILD_PASTEC_EXE=ON \
    -DBUILD_EXAMPLES=OFF \
    -DSIMSIMD_BUILD_SHARED=ON \
    -DINSTALL_TARGETS=OFF \
    -DUSE_MIMALLOC=ON \
    && make -j$(nproc) VERBOSE=1

# Copy the visual words data file to the data directory
RUN cp /app/visualWordsORB.dat /app/data/

# Set up the final structure
WORKDIR /app

EXPOSE 4212

VOLUME /app/data

CMD ["/app/build/pastec_server", "--forward-index", "-p", "4212", "/app/data/visualWordsORB.dat"]
