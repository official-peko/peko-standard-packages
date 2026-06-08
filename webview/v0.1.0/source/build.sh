alias peko=/Users/preston/Work/peko/dev/tools/peko_cli/target/debug/peko

clang++ $(peko clangflags --os=windows --arch=x86_64) desktop/desktop_webview.cc -o ../objects/windows/webview.o

clang++ $(peko clangflags --os=macos --arch=x86_64) desktop/desktop_webview.cc -o ../objects/macos/x86_64/webview.o
clang++ $(peko clangflags --os=macos --arch=arm) desktop/desktop_webview.cc -o ../objects/macos/arm/webview.o

clang++ $(peko clangflags --os=linux --arch=x86_64) desktop/desktop_webview.cc -o ../objects/linux/x86_64/webview.o
clang++ $(peko clangflags --os=linux --arch=arm) desktop/desktop_webview.cc -o ../objects/linux/arm/webview.o

clang $(peko clangflags --nostd --os=ios --arch=x86_64) ios/ios_webview.m -o ../objects/ios/x86_64/webview.o
clang $(peko clangflags --nostd --os=ios --arch=arm) ios/ios_webview.m -o ../objects/ios/arm/webview.o

clang $(peko clangflags --nostd --os=android --arch=arm) android/android_webview.c -o ../objects/android/webview.o
