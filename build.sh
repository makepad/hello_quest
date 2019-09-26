#!/bin/bash
PATH=$PATH:$ANDROID_HOME/build-tools/28.0.3
PATH=$PATH:$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin

find build -mindepth 1 -delete
mkdir -p build
pushd build > /dev/null
javac -classpath $ANDROID_HOME/platforms/android-26/android.jar -d . ../src/main/java/com/makepad/hello_world/*.java
dx --dex --output classes.dex .
mkdir -p lib/arm64-v8a
pushd lib/arm64-v8a > /dev/null
aarch64-linux-android26-clang\
    -march=armv8-a\
    -shared\
    -I $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/\
    -I $OVR_HOME/VrApi/Include\
    -L $ANDROID_NDK_HOME/platforms/android-26/arch-arm64/usr/lib\
    -L $OVR_HOME/VrApi/Libs/Android/arm64-v8a/Debug\
    -landroid\
    -llog\
    -lvrapi\
    -o libmain.so\
   ../../../src/main/cpp/*.c
cp $OVR_HOME/VrApi/Libs/Android/arm64-v8a/Debug/libvrapi.so .
popd > /dev/null
aapt package -F hello_world.apk -I $ANDROID_HOME/platforms/android-26/android.jar -M ../src/main/AndroidManifest.xml -f
aapt add hello_world.apk classes.dex
aapt add hello_world.apk lib/arm64-v8a/libmain.so
aapt add hello_world.apk lib/arm64-v8a/libvrapi.so
apksigner sign -ks ~/.android/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android hello_world.apk
popd > /dev/null
