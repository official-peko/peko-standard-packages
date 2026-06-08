#!/bin/zsh
alias peko=/Users/preston/Work/peko/dev/tools/peko_cli/target/debug/peko

clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android_webview.c                 -o $(pwd)/../../libs/android/android_webview.o; \
clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android/android_native_app_glue.c -o $(pwd)/../../libs/android/native_glue.o; \
clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android/android_usb_devices.c     -o $(pwd)/../../libs/android/android_usb_devices.o; \
