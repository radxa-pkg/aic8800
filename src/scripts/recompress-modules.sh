#!/bin/bash

# fix: https://bugs.gentoo.org/920837

KMOD_ISSUE_VERSION="31"
DKMS_ISSUE_VERSION="3.2.0"
MODULE_DKMS_DIR="/var/lib/dkms/$module/$module_version/$(uname -r)/$(uname -p)/module"

kmod_version=$(kmod --version | head -n 1 | awk '{print $3}')
dkms_version=$(dkms --version | awk -F'-' '{print $2}')

if [ "$(printf '%s\n' $DKMS_ISSUE_VERSION $dkms_version | sort -rV | head -n1)" != "$dkms_version" ] && [ "$kmod_version" -ge "$KMOD_ISSUE_VERSION" ] ; then
    find "$MODULE_DKMS_DIR" -name "*.ko.xz" -print0 | while IFS= read -r -d '' module_file; do
        /usr/bin/xz -d ${module_file}
        /usr/bin/xz --check=crc32 --lzma2=dict=1MiB -f ${module_file%.xz}
    done
fi
