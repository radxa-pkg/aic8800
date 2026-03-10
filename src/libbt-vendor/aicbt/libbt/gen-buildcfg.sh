#!/usr/bin/env bash

if [[ "" == "$2" ]]
then
    echo "Usage: $0 <in-file> <out-file>"
    exit 1
fi

if [ ! -f "$1" ]
then
    echo "Error: Can't find input file $1..."
    exit 2
fi

DATE=`/usr/bin/env date`
BASE=`basename $2`
BASE=`echo ${BASE} | tr "[:lower:]" "[:upper:]"`
BASE=`echo ${BASE} | sed -e "s/\\./_/"`
PROTECT="_${BASE}"

echo "/* Auto-generated from $1 on ${DATE} */" > $2
echo "#ifndef ${PROTECT}" >> $2
echo "#define ${PROTECT}" >> $2
sed -e 's/\s\+$//g;s/^\s\+//g' -e '/^#/d' -e '/^$/d' -e '/# Makefile only$/d' -e 's/^/#define /' -e 's/=/ /' $1 >> $2
echo "#endif" >> $2
