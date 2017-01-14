#!/usr/bin/env bash
set -e

JDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$JDIR"/util.sh

set -x

pushd "${CACHE_DIR:-/tmp}" >/dev/null

INSTALLED_VERSION=$((cd NFD && git rev-parse HEAD) 2>/dev/null || echo NONE)

sudo rm -Rf NFD-latest

git clone git://github.com/named-data/NFD NFD-latest

LATEST_VERSION=$((cd NFD-latest && git rev-parse HEAD) 2>/dev/null || echo UNKNOWN)

if [[ $INSTALLED_VERSION != $LATEST_VERSION ]]; then
    sudo rm -Rf NFD
    mv NFD-latest NFD
else
    sudo rm -Rf NFD-latest
fi

sudo killall nfd || true

sudo rm -f /usr/local/bin/nfd
sudo rm -f /usr/local/etc/ndn/nfd.conf

pushd NFD >/dev/null

git submodule init
git submodule sync
git submodule update

./waf -j1 --color=yes configure
./waf -j1 --color=yes build
sudo ./waf -j1 --color=yes install

# Install default config
sudo cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf

popd >/dev/null
popd >/dev/null
