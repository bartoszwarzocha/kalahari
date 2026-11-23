#!/bin/bash
# The Linux packages of Kalahari installed on a clean Ubuntu 24.04 (the base of Linux Mint 22)
# as a writer installs them, in a container of docker:
#
#   packaging/linux/check-packages.sh DIST
#
# DIST holds the .deb and the AppImage of scripts/package.py linux. The script runs itself in
# the container with --inside FOLDER (FOLDER: DIST in the container), where:
#   1. apt installs the .deb: its Depends name every library of the system it needs
#   2. every program and library in /opt/kalahari finds its libraries (ldd)
#   3. the program passes its check of an installation (kalahari --self-test) as a writer, not
#      root: without a screen and with an X server (xcb, the platform of a desktop); its
#      Python is the one in /opt/kalahari
#   4. apt removes the package and leaves none of its files
#   5. the AppImage, on the same system, starts as on a desktop (FUSE) and passes the check
#
# apt in the container downloads from the mirror of Ubuntu that the machine running docker
# uses (a runner of GitHub has one next to it), not from archive.ubuntu.com, which can be slow
# from there
set -euo pipefail

IMAGE="ubuntu:24.04"

# apt_mirror < SOURCES: the first mirror of the archive of Ubuntu in sources of apt
apt_mirror() {
    grep -v '^[[:space:]]*#' | grep -oE 'http://[^[:space:]]+/ubuntu/?([[:space:]]|$)' |
        sed 's/[[:space:]]*$//' | grep -v security | head -1
}

if [ "${1:-}" != "--inside" ]; then
    dist=$(cd "${1:?Give the folder of the packages}" && pwd)
    script="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
    fuse=()
    if [ -e /dev/fuse ]; then
        # The AppImage mounts itself with FUSE, as on a desktop
        fuse=(--device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor=unconfined)
    fi
    # The first mirror of the archive of Ubuntu in the sources of apt of this machine (http:
    # the clean system has no certificates for https)
    mirror=$(cat /etc/apt/apt-mirrors.txt /etc/apt/sources.list.d/*.sources \
                 /etc/apt/sources.list 2>/dev/null | apt_mirror || true)
    exec docker run --rm "${fuse[@]}" -e APT_MIRROR="$mirror" \
        -v "$dist:/packages:ro" -v "$script:/check-packages.sh:ro" \
        "$IMAGE" bash /check-packages.sh --inside /packages
fi
packages=${2:?Give the folder of the packages}

fail() {
    echo "check-packages.sh: $*" >&2
    exit 1
}
step() {
    echo
    echo "=== $*"
}
apt_get() {
    apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=30 "$@"
}

shopt -s nullglob
debs=("$packages"/kalahari_*_amd64.deb)
images=("$packages"/Kalahari-*-x86_64.AppImage)
[ ${#debs[@]} -eq 1 ] || fail "${#debs[@]} .deb packages in the folder, not one"
[ ${#images[@]} -eq 1 ] || fail "${#images[@]} AppImages in the folder, not one"
export DEBIAN_FRONTEND=noninteractive

# self_test PYTHON_FOLDER COMMAND...: the check of an installation as the writer, in a new home;
# the Python of the program must be in PYTHON_FOLDER ("" for any folder outside /usr). The
# report of the check goes to the log and to the output
self_test() {
    local within=$1
    shift
    local home output python
    home=$(runuser -u writer -- mktemp -d)
    if ! output=$(runuser -u writer -- env -i HOME="$home" PATH=/usr/bin:/bin LANG=C.UTF-8 \
            "$@" --self-test 2>&1); then
        echo "$output" >&2
        fail "the check of the installation failed: $*"
    fi
    echo "$output" >&2
    python=$(printf '%s\n' "$output" | sed -n 's/^OK *Python: .*] (\(.*\))$/\1/p')
    [ -n "$python" ] || fail "no folder of Python in the check"
    case "$python" in
        /usr | /usr/*) fail "the program uses the Python of the system ($python)" ;;
    esac
    if [ -n "$within" ]; then
        case "$python" in
            "$within" | "$within"/*) ;;
            *) fail "the Python of the program is not in $within ($python)" ;;
        esac
    fi
    printf '%s\n' "$output"
}

step "apt installs the package and the libraries of the system it names"
if [ -n "${APT_MIRROR:-}" ]; then
    sed -i -E "s#https?://archive\.ubuntu\.com/ubuntu/?#${APT_MIRROR%/}/#g" \
        /etc/apt/sources.list.d/ubuntu.sources
fi
echo "Mirror of Ubuntu: $(grep -m1 -oE 'https?://[^[:space:]]+' /etc/apt/sources.list.d/ubuntu.sources)"
apt_get update -qq
cp "${debs[0]}" /tmp/kalahari.deb
chmod 644 /tmp/kalahari.deb
apt_get install -y --no-install-recommends /tmp/kalahari.deb
echo "Depends: $(dpkg-query -W -f='${Depends}' kalahari)"

step "Every program and library of the package finds its libraries"
# As the system loads them: by the RUNPATH of each file, then in the folders of the system. A
# library a file does not find is missing from the system, or the package has it where the
# file does not look for it (a library loaded before by the program hid that when the program
# was built)
checked=0
missing=0
while IFS= read -r -d '' file; do
    [ "$(head -c 4 "$file" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ] || continue
    checked=$((checked + 1))
    while read -r name _; do
        missing=1
        if [ -n "$(find /opt/kalahari -name "$name" -print -quit)" ]; then
            echo "$file: $name not found, although the package has it"
        else
            echo "$file: $name not found, the system does not have it"
        fi
    done < <(env -u LD_LIBRARY_PATH ldd "$file" 2>/dev/null | grep "=> not found" || true)
done < <(find /opt/kalahari -type f -print0)
[ $missing -eq 0 ] || fail "libraries not found"
echo "$checked programs and libraries: every library found"

# on_x_server REPORT: the report of a check names the platform of an X server
on_x_server() {
    grep "^OK *Qt platform: xcb" > /dev/null <<< "$1" || fail "the program did not use the X server (xcb)"
}

step "The check of the installation, as a writer, without a screen"
useradd --create-home writer
self_test /opt/kalahari env QT_QPA_PLATFORM=offscreen kalahari > /dev/null

step "The check of the installation, as a writer, with an X server (xcb)"
apt_get install -y --no-install-recommends xvfb xauth
report=$(self_test /opt/kalahari xvfb-run -a kalahari)
on_x_server "$report"

step "apt removes the package"
installed=$(mktemp)
dpkg -L kalahari | while IFS= read -r path; do
    if [ -L "$path" ] || [ -f "$path" ]; then
        echo "$path"
    fi
done > "$installed"
echo "$(wc -l < "$installed") files of the package"
apt_get remove -y kalahari
left=0
while IFS= read -r path; do
    if [ -e "$path" ] || [ -L "$path" ]; then
        echo "left: $path"
        left=1
    fi
done < "$installed"
if [ -e /opt/kalahari ]; then
    find /opt/kalahari | head -20
    left=1
fi
[ $left -eq 0 ] || fail "apt left files of the package"
echo "nothing of the package left"

step "The AppImage on the same system"
appimage=/home/writer/Kalahari.AppImage
cp "${images[0]}" "$appimage"
chown writer: "$appimage"
chmod 755 "$appimage"
if [ -e /dev/fuse ]; then
    apt_get install -y --no-install-recommends fuse3
    start=("$appimage")
else
    echo "No FUSE in this machine: the AppImage unpacks itself instead"
    start=(env APPIMAGE_EXTRACT_AND_RUN=1 "$appimage")
fi
self_test "" env QT_QPA_PLATFORM=offscreen "${start[@]}" > /dev/null
report=$(self_test "" xvfb-run -a "${start[@]}")
on_x_server "$report"

echo
echo "check-packages.sh: the .deb and the AppImage work on a clean Ubuntu 24.04"
