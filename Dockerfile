# Dockerizing Hybserv2: Dockerfile for building Hybserv2 images
# Based on ubuntu:latest

FROM       ubuntu:latest
MAINTAINER Dinko Korunic <dinko.korunic@gmail.com>

# install required packages
RUN apt-get update && apt-get install -y build-essential git-core

# install hybserv2
RUN mkdir -p /root/src
RUN git clone https://github.com/dkorunic/hybserv2.git /root/src/hybserv2
RUN cd /root/src/hybserv2 && ./configure --enable-daemontools && make all install
RUN useradd --system hybserv
RUN chown -Rh hybserv: /usr/local/hybserv

# clean cruft
RUN rm -rf /root/src
RUN apt-get -y purge build-essential git-core fakeroot libfakeroot patch cpp make binutils libc-dev-bin libc6-dev linux-libc-dev manpages
RUN apt-get -y autoremove

# add volumes
VOLUME ["/usr/local/hybserv"]

# expose required ports
EXPOSE 6667 6800

# default cmd
CMD ["su", "-c", "/usr/local/hybserv/hybserv", "-", "hybserv"]
