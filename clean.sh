#!/bin/bash

set -e

make distclean 2> /dev/null || true
rm -rf .deps .libs _install Makefile Makefile.in aclocal.m4 autom4te.cache/ autoscan-2.71.log compile config.* configure configure~ configure.scan
rm -rf depcomp install-sh libtool ltmain.sh missing stamp-h1 *.o *.la *.lo *.exe

