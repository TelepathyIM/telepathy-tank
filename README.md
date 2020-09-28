# Telepathy-Tank

[![Build Status](https://travis-ci.com/TelepathyIM/telepathy-tank.svg?branch=master)](https://travis-ci.com/TelepathyIM/telepathy-tank)

Tank is a Qt-based matrix connection operator for the Telepathy framework.

## Features

## Requirements

* CMake 3.2 (required by TelepathyQt)
* Qt 5.6
* [TelepathyQt](https://github.com/TelepathyIM/telepathy-qt)
* [libQuotient](https://github.com/quotient-im/libQuotient)

Note: In order to use Tank, you need to have a complementary Telepathy Client application, such as KDE-Telepathy or Empathy.

## Installation

### Arch Linux AUR

[AUR package for git version](https://aur.archlinux.org/packages/telepathy-tank-git/):

    yaourt -S telepathy-tank-git

### Manually

    git clone https://github.com/TelepathyIM/telepathy-tank.git

or

    tar -xf telepathy-tank-0.1.0.tar.gz

    mkdir tank-build
    cd tank-build
    cmake ../telepathy-tank

### Information about CMake build:

Default installation prefix is `/usr/local`. Probably, you'll need to set `CMAKE_INSTALL_PREFIX` to `/usr` to make DBus activation works. (`-DCMAKE_INSTALL_PREFIX=/usr`)

    make -j4
    make install

## Known issues

## License

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

See COPYNG for details.
