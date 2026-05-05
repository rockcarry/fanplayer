#!/bin/bash

set -e

aclocal
autoheader
libtoolize --automake --copy
automake --foreign --add-missing --copy
autoconf
