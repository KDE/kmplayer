#! /usr/bin/env bash
$EXTRACTRC $(find . -name '*.rc') >> rc.cpp || exit 11
$EXTRACTRC $(find . -name '*.ui') >> rc.cpp || exit 12
$EXTRACTRC $(find . -name '*.kcfg') >> rc.cpp
$XGETTEXT lib/*.cpp part/*.cpp app/*.cpp rc.cpp -o $podir/kmplayer.pot
rm -f rc.cpp
