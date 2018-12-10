FROM ubuntu:latest

RUN apt-get update \
  && apt-get install -y \
    libssl-dev \
    autoconf \
    automake \
    libtool \
    python \
    python-dev \
    python3-dev \
    python-setuptools \
    libpcre2-dev \
    libpcre3-dev \
    pkg-config

ADD . /watchman/

WORKDIR /watchman

RUN ./autogen.sh \
  && ./configure \
  && make \
  && make install

WORKDIR /
RUN rm -rf /watchman
