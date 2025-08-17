# The virtme-ng present in the latest LTS at the time of writing has a bug that causes
# the VM to shutdown unexpectedly.
FROM ubuntu:25.04

# deb-src repositories are required for `apt-get build-dep`.
RUN sed -i 's/Types: deb/Types: deb deb-src/g' /etc/apt/sources.list.d/ubuntu.sources

# iproute2, kbd and udev are required by virtme-ng
# nodejs is needed for the GitHub Actions cache scripts
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y virtme-ng iproute2 kbd udev fio nodejs && \
    apt-get build-dep -y --no-install-recommends linux
WORKDIR /usr/src/
ADD https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.5.7.tar.xz /usr/src/
RUN tar xf linux-6.5.7.tar.xz
WORKDIR /usr/src/linux-6.5.7/
RUN vng --configitem CONFIG_DEBUG_KMEMLEAK=y --build
