#!/bin/sh
debnam=opensbi
debver=`grep -m1 "^${debnam}" debian/changelog | cut -d '(' -f 2 | cut -d ')' -f 1 | cut -d '-' -f 1`

if [ ! -e ../${debnam}_${debver}.orig.tar.xz ]; then
  tar --exclude=debian --exclude-vcs -cJf ../${debnam}_${debver}.orig.tar.xz .
fi

debuild -us -uc
