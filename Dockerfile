FROM ubuntu:latest

CMD apt install libssl-dev \
  && autoconf \
  && automake \
  && libtool \
  && pcre \
  && python \
  && python-dev \
  && setuptools

ADD ./* /watchman/

WORKDIR /watchman

CMD ./autogen.sh \
  && ./configure \
  && make \
  && make install

WORKDIR /
CMD rm -rf /watchman
