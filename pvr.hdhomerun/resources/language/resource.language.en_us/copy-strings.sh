#!/bin/sh

pushd $(dirname $0) >> /dev/null
thisdir=$(basename $(pwd))

for i in ../*; do
   if [[ $(basename $i) != $thisdir ]]; then
       echo $(basename $i)
       cp strings.po $i/strings.po
   fi
done
