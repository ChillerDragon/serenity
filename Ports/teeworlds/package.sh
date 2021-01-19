#!/bin/bash ../.port_include.sh
port=teeworlds
version=git
workdir=teeworlds-master
useconfigure=true
files="https://github.com/teeworlds/teeworlds/archive/master.tar.gz teeworlds-master.tar.gz
https://github.com/teeworlds/teeworlds-maps/archive/master.tar.gz teeworlds-maps-master.tar.gz
https://github.com/teeworlds/teeworlds-translation/archive/master.tar.gz teeworlds-translation-master.tar.gz"
configopts="-DCMAKE_TOOLCHAIN_FILE=$SERENITY_ROOT/Toolchain/CMakeToolchain.txt"
depends="SDL2 freetype2"

update_submodule() {
    local src="$1"
    local dst="$2"
    if [ -d "$dst" ]; then rm -rf "$dst"; fi

    cp -r "$src" "$dst"
}

configure() {
    update_submodule teeworlds-maps-master teeworlds-master/datasrc/maps
    update_submodule teeworlds-translation-master teeworlds-master/datasrc/languages
    run cmake $configopts
}

install() {
    # run make install
    test
}

