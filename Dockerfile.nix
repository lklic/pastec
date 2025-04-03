# Nix builder stage
FROM nixos/nix:latest AS builder

# Create working directory
WORKDIR /app

# Find the path to the CA certificates
RUN echo "Locating CA certificates..."
RUN nix-build '<nixpkgs>' -A cacert --no-out-link > /tmp/cacert-path

# Copy our source WITHOUT the .git directory
# We'll use a more selective copy approach
COPY CMakeLists.txt flake.nix shell.nix ./
COPY cmake ./cmake/
COPY include ./include/
COPY python ./python/
COPY src ./src/
COPY examples ./examples/
COPY visualWordsORB.dat ./

# Build our Nix environment
RUN nix \
    --extra-experimental-features "nix-command flakes" \
    --option filter-syscalls false \
    build .

# Copy the Nix store closure into a directory
RUN mkdir /tmp/nix-store-closure
RUN cp -R $(nix-store -qR result/) /tmp/nix-store-closure

# Copy CA certificates to a known location
RUN mkdir -p /tmp/etc/ssl/certs
RUN cp -L $(cat /tmp/cacert-path)/etc/ssl/certs/ca-bundle.crt /tmp/etc/ssl/certs/

# Final minimal image
FROM scratch

# Set environment variables for memory allocator optimizations
ENV MIMALLOC_LARGE_OS_PAGES=1
ENV MALLOC_CONF="thp:always,metadata_thp:always"
ENV GLIBC_TUNABLES=glibc.malloc.hugetlb=1
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-bundle.crt
ENV CURL_CA_BUNDLE=/etc/ssl/certs/ca-bundle.crt

# Copy /nix/store and our built application
COPY --from=builder /tmp/nix-store-closure /nix/store
COPY --from=builder /app/result /app
COPY --from=builder /tmp/etc /etc

# Create data directory and expose port
WORKDIR /app
VOLUME /app/data
EXPOSE 4212

# Set the command to run Pastec
CMD ["/app/bin/pastec", "-p", "4212", "/app/data/visualWordsORB.dat"]
