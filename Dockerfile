FROM debian:12-slim as builder

ARG DPDK_VER=22.11
ARG DPSERVICE_FEATURES=""

WORKDIR /workspace

# Install prerequisite packages
RUN apt-get update && apt-get upgrade && apt-get install -y --no-install-recommends ON \
libibverbs-dev \
libmnl-dev \
libnuma-dev \
numactl \
libnuma1 \
unzip \
wget \
make \
gcc \
g++ \
clang \
git \
ethtool \
pciutils \
procps \
ninja-build \
meson \
python3-pyelftools \
iproute2 \
libuuid1 \
uuid-dev \
net-tools \
xz-utils \
tar \
findutils \
jq \
curl \
build-essential \
pkg-config \
protobuf-compiler-grpc \
libgrpc++1.51 \
libgrpc++-dev \
linux-headers-${OSARCH} \
&& rm -rf /var/lib/apt/lists/*

# Download DPDK
RUN wget http://git.dpdk.org/dpdk/snapshot/dpdk-${DPDK_VER}.zip
RUN unzip dpdk-${DPDK_VER}.zip

ENV DPDK_DIR=/workspace/dpdk-${DPDK_VER}

# Copy DPDK patches
COPY hack/*.patch hack/
RUN cd $DPDK_DIR && patch -p1 < ../hack/dpdk_22_11_gcc12.patch
RUN cd $DPDK_DIR && patch -p1 < ../hack/dpdk_22_11_log.patch
RUN cd $DPDK_DIR && patch -p1 < ../hack/dpdk_22_11_telemetry_key.patch

# Compile DPDK
RUN cd $DPDK_DIR && meson setup -Dmax_ethports=132 -Dplatform=generic -Ddisable_drivers=common/dpaax,\
common/cpt,common/iavf,\
common/octeontx,common/octeontx2,common/cnxk,common/qat,regex/octeontx2,net/cnxk,dma/cnxk,\
common/sfc_efx,common/auxiliary,common/dpaa,common/fslmc,common/ifpga,common/vdev,common/vmbus,\
mempool/octeontx,mempool/octeontx2,baseband/*,event/*,net/ark,net/atlantic,net/avp,net/axgbe,\
net/bnxt,net/bond,net/cxgbe,net/dpaa,net/dpaa2,net/e1000,net/ena,net/enetc,net/enetfec,net/enic,\
net/failsafe,net/fm10k,net/hinic,net/hns3,net/i40e,net/iavf,net/ice,net/igc,net/ionic,net/ipn3ke,\
net/ixgbe,net/liquidio,net/memif,net/netsvs,net/nfp,net/ngbe,net/null,net/octeontx,net/octeontx2,\
net/octeontx_ep,net/pcap,net/pfe,net/qede,net/sfc,net/softnic,net/thunderx,net/txgbe,\
net/vdev_ntsvc,net/vhost,net/virtio,net/vmxnet3,net/bnx2x,net/netsvc,net/vdev_netsvc,\
crypto/dpaa_sec,crypto/bcmfs,crypto/caam_jr,crypto/cnxk,dpaa_sec,crypto/dpaa2_sec,crypto/nitrox,\
crypto/null,crypto/octeontx,crypto/octeontx2,crypto/scheduler,crypto/virtio -Ddisable_libs=power,\
vhost,gpudev build -Ddisable_apps="*" -Dtests=false
RUN cd $DPDK_DIR/build && ninja
RUN cd $DPDK_DIR/build && ninja install

# Get companion binaries from other repos
ADD https://github.com/ironcore-dev/dpservice-cli/releases/download/v0.1.7/github.com.onmetal.dpservice-cli_0.1.7_linux_amd64.tar.gz dpservice-cli.tgz
RUN tar -xzf dpservice-cli.tgz

# Now copy the rest to enable DPDK layer caching
COPY meson.build meson.build
COPY meson_options.txt meson_options.txt
COPY src/ src/
COPY include/ include/
COPY test/ test/
COPY hack/* hack/
COPY proto/ proto/
COPY tools/ tools/
# Needed for version extraction by meson
COPY .git/ .git/

RUN meson setup build $DPSERVICE_FEATURES && cd ./build && ninja

FROM builder AS testbuilder
RUN rm -rf build && meson setup build $DPSERVICE_FEATURES --buildtype=release && cd ./build && ninja
RUN rm -rf build && CC=clang CXX=clang++ meson setup build $DPSERVICE_FEATURES && cd ./build && ninja

FROM debian:12-slim as tester

RUN apt-get update && apt-get install -y --no-install-recommends ON \
libibverbs-dev \
numactl \
libnuma1 \
pciutils \
procps \
libuuid1 \
libgrpc++1.51 \
iproute2 \
udev \
gawk \
python3 \
python3-pytest \
python3-scapy \
&& rm -rf /var/lib/apt/lists/*

WORKDIR /
COPY --from=testbuilder /workspace/test ./test
COPY --from=testbuilder /workspace/build/src/dp_service ./build/src/dp_service
COPY --from=testbuilder /workspace/github.com/onmetal/dpservice-cli ./build
COPY --from=testbuilder /usr/local/lib /usr/local/lib
RUN ldconfig

WORKDIR /test
ENTRYPOINT ["pytest-3", "-x", "-v"]

FROM debian:12-slim as production

RUN apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends ON \
libibverbs-dev \
numactl \
libnuma1 \
pciutils \
procps \
libuuid1 \
libgrpc++1.51 \
iproute2 \
udev \
gawk \
bash-completion \
&& rm -rf /var/lib/apt/lists/*

WORKDIR /
COPY --from=builder /workspace/build/src/dp_service \
					/workspace/build/tools/dp_grpc_client \
					/workspace/build/tools/dpservice-dump \
					/workspace/github.com/onmetal/dpservice-cli \
					/workspace/hack/prepare.sh \
					/usr/local/bin
COPY --from=builder /usr/local/lib /usr/local/lib
RUN ldconfig

# Ensure bash-completion is working in operations
RUN echo 'PATH=${PATH}:/\nsource /etc/bash_completion\nsource <(dpservice-cli completion bash)' >> /root/.bashrc

ENTRYPOINT ["dp_service"]
