#! /bin/bash

test -d ./config || mkdir ./config


printf "dist_man1_MANS = "> ./man/Makefile.am
find ./man/*.1 -printf "%f " >> ./man/Makefile.am
printf "\ndist_man3_MANS = ">> ./man/Makefile.am
find ./man/*.3 -printf "%f " >> ./man/Makefile.am

autoreconf -vif
