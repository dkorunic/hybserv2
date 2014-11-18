# Dockerizing Hybserv2: Dockerfile for building Hybserv2 images
# Based on ubuntu:latest

FROM       ubuntu:latest
MAINTAINER Dinko Korunic <dinko.korunic@gmail.com>

# install required packages
RUN apt-get update && apt-get install -y build-essential git-core

# install hybserv2
RUN mkdir -p /root/src \
    && git clone https://github.com/dkorunic/hybserv2.git /root/src/hybserv2 \
    && cd /root/src/hybserv2 && ./configure --enable-daemontools \
    && make all install \
    && useradd --system hybserv \
    && chown -Rh hybserv: /usr/local/hybserv \
    && rm -rf /root/src \
    && apt-get -y purge build-essential git-core fakeroot libfakeroot patch cpp make binutils libc-dev-bin libc6-dev linux-libc-dev manpages \
    && apt-get -y autoremove \
    && rm -rf /var/lib/apt/lists/*

# add volumes
VOLUME ["/usr/local/hybserv"]

# expose required ports
EXPOSE 6667 6800

# default cmd
CMD ["su", "-c", "/usr/local/hybserv/hybserv", "-", "hybserv"]
