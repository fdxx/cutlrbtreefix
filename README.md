## About

- Fix "CUtlRBTree overflow" on level transition. extracted from [sourcetvsupport](https://github.com/shqke/sourcetvsupport).
- Auto reset current map when string pool limit is exceeded. (During the game, the level transition has not yet been performed).

## Requirements:

- L4D1/2 dedicated server
- Sourcemod 1.10 or later

## Build manually
```sh
## Debian as an example.
dpkg --add-architecture i386
apt update
apt install -y clang g++-multilib python3 python3-pip git

mkdir temp && cd temp

git clone --depth=1 -b 1.11-dev --recurse-submodules https://github.com/alliedmodders/sourcemod sourcemod
git clone --depth=1 -b 1.11-dev https://github.com/alliedmodders/metamod-source metamod
git clone --depth=1 -b l4d2 https://github.com/alliedmodders/hl2sdk hl2sdk-l4d2
git clone --depth=1 https://github.com/fdxx/cutlrbtreefix

python3 -m pip install wheel
pip install git+https://github.com/alliedmodders/ambuild

cd cutlrbtreefix
mkdir build && cd build

export CC=clang && export CXX=clang++

python3 ../configure.py --enable-optimize --sm-path="../../sourcemod" --mms-path="../../metamod"

ambuild

## Check the cutlrbtreefix/build/package folder.
```
