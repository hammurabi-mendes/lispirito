#!/bin/sh

if [ $# -lt 1 ]; then
    echo "usage: make_disk.sh <diskname> [file1 ... fileN]"
    exit 1
fi

DISKNAME=$1
shift

c1541 -format ${DISKNAME},00 d64 ${DISKNAME}.d64

for ARG in "$@"; do
    case "$ARG" in
        *,S|*,s)
            FILE="${ARG%,*}"
            c1541 ${DISKNAME}.d64 -write "${FILE}" "${FILE},s"
            ;;
        *,P|*,p)
            FILE="${ARG%,*}"
            c1541 ${DISKNAME}.d64 -write "${FILE}" "${FILE},p"
            ;;
        *)
            c1541 ${DISKNAME}.d64 -write "${ARG}"
            ;;
    esac
done
