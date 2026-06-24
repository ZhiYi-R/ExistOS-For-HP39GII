#!/bin/sh -e
source $stdenv/setup

mkdir -p $out

cd $src
cp -R $src $TMP/ExistOS
chmod -R 755 $TMP/ExistOS
cd $TMP/ExistOS

echo '#!/bin/sh -e
sys_signer $@' >> ./tools/sys_signer
chmod +x ./tools/sys_signer

echo '#!/bin/sh -e
elftosb $@' >> ./tools/sbtools/elftosb
chmod +x ./tools/sbtools/elftosb

cmake .
make
cp $TMP/ExistOS/Bootloader/OSLoader.sb $out/
cp $TMP/ExistOS/System/ExistOS.sys $out/
