#!/bin/zsh
#
# build.sh
# Per-package build for the storage package.
# The data path resolver and the keychain backend each have one source per
# platform. The SQLite backend has one source for every platform. The Peko
# link blocks expect three object names: peko_storage, peko_storage_path, and
# peko_keychain. This script compiles the source that matches each platform and
# writes it to the matching object name in the library tree.

alias peko=/Users/preston/Work/peko-dev/peko-tools/target/debug/peko

# Directory of this script, which also holds the package sources.
HERE="${0:A:h}"
# Output library tree, matching the generic build layout.
LIBS="$HERE/../libs"

# Include path for the libsodium headers (sodium.h), used by the Linux and
# Android keychain backends. Set this when sodium.h is not already on the
# include path that peko clangflags provides.
SODIUM_INC="$HERE/libsodium"

# Include path for the libsecret headers (libsecret/secret.h), used by the
# Linux keychain backend at build time only. libsecret is loaded with dlopen at
# run time and is not linked. Set this when the headers are not already on the
# include path.
SECRET_INC=""

# Create the output directories.
mkdir -p \
    "$LIBS/macos/x86_64" "$LIBS/macos/arm" \
    "$LIBS/ios/x86_64"   "$LIBS/ios/arm" \
    "$LIBS/linux/x86_64" "$LIBS/linux/arm" \
    "$LIBS/windows" \
    "$LIBS/android"

# --- helpers -----------------------------------------------------------------

# objc_apple SRC NAME : compile an Objective-C source for macos and ios.
objc_apple() {
    clang $(peko clangflags --nostd --os=macos --arch=x86_64) -x objective-c -fobjc-arc -std=gnu11 "$1" -o "$LIBS/macos/x86_64/$2.o"
    clang $(peko clangflags --nostd --os=macos --arch=arm)    -x objective-c -fobjc-arc -std=gnu11 "$1" -o "$LIBS/macos/arm/$2.o"
    clang $(peko clangflags --nostd --os=ios --arch=x86_64)   -x objective-c -fobjc-arc -std=gnu11 "$1" -o "$LIBS/ios/x86_64/$2.o"
    clang $(peko clangflags --nostd --os=ios --arch=arm)      -x objective-c -fobjc-arc -std=gnu11 "$1" -o "$LIBS/ios/arm/$2.o"
}

# cc_apple SRC NAME : compile a C source for macos and ios.
cc_apple() {
    clang $(peko clangflags --nostd --os=macos --arch=x86_64) -std=c17 "$1" -o "$LIBS/macos/x86_64/$2.o"
    clang $(peko clangflags --nostd --os=macos --arch=arm)    -std=c17 "$1" -o "$LIBS/macos/arm/$2.o"
    clang $(peko clangflags --nostd --os=ios --arch=x86_64)           "$1" -o "$LIBS/ios/x86_64/$2.o"
    clang $(peko clangflags --nostd --os=ios --arch=arm)              "$1" -o "$LIBS/ios/arm/$2.o"
}

# cc_linux SRC NAME [EXTRA_FLAGS] : compile a C source for linux x86_64 and arm.
cc_linux() {
    clang $(peko clangflags --nostd --os=linux --arch=x86_64) ${=3} "$1" -o "$LIBS/linux/x86_64/$2.o"
    clang $(peko clangflags --nostd --os=linux --arch=arm)    ${=3} "$1" -o "$LIBS/linux/arm/$2.o"
}

# cc_windows SRC NAME : compile a C source for windows x86_64.
cc_windows() {
    clang $(peko clangflags --nostd --os=windows --arch=x86_64) "$1" -o "$LIBS/windows/$2.o"
}

# cc_android SRC NAME [EXTRA_FLAGS] : compile a C source for android arm.
cc_android() {
    clang $(peko clangflags --nostd --os=android --arch=arm) ${=3} "$1" -o "$LIBS/android/$2.o"
}

# --- sqlite3 : vendored SQLite amalgamation for every platform ---------------

echo "=== sqlite3 ==="
echo "--- Compiling for macos and ios ---"
cc_apple   "$HERE/sqlite3.c" sqlite3
echo "--- Compiling for linux ---"
cc_linux   "$HERE/sqlite3.c" sqlite3
echo "--- Compiling for windows ---"
cc_windows "$HERE/sqlite3.c" sqlite3
echo "--- Compiling for android ---"
cc_android "$HERE/sqlite3.c" sqlite3

# --- peko_storage : shared SQLite backend for every platform -----------------

echo "=== peko_storage ==="
echo "--- Compiling for macos and ios ---"
cc_apple   "$HERE/peko_storage.c" peko_storage
echo "--- Compiling for linux ---"
cc_linux   "$HERE/peko_storage.c" peko_storage
echo "--- Compiling for windows ---"
cc_windows "$HERE/peko_storage.c" peko_storage
echo "--- Compiling for android ---"
cc_android "$HERE/peko_storage.c" peko_storage

# --- peko_storage_path : per-platform data directory resolver ----------------

echo "=== peko_storage_path ==="
echo "--- Compiling for macos and ios ---"
objc_apple "$HERE/peko_storage_path_apple.m"   peko_storage_path
echo "--- Compiling for linux ---"
cc_linux   "$HERE/peko_storage_path_linux.c"   peko_storage_path
echo "--- Compiling for windows ---"
cc_windows "$HERE/peko_storage_path_windows.c" peko_storage_path
echo "--- Compiling for android ---"
cc_android "$HERE/peko_storage_path_android.c" peko_storage_path

# --- peko_keychain : per-platform secure credential backend ------------------

echo "=== peko_keychain ==="
echo "--- Compiling for macos and ios ---"
objc_apple "$HERE/peko_keychain_apple.m"     peko_keychain
echo "--- Compiling for linux ---"
cc_linux   "$HERE/peko_keychain_linux.c"     peko_keychain "$SECRET_INC $SODIUM_INC"
echo "--- Compiling for windows ---"
cc_windows "$HERE/peko_keychain_windows.c"   peko_keychain
echo "--- Compiling for android ---"
cc_android "$HERE/peko_keychain_android.c"   peko_keychain "$SODIUM_INC"
