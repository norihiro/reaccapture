#! /bin/sh

ln -s README.md README
aclocal
autoheader 
autoconf
automake --add-missing
