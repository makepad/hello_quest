# Hello Quest

This is a minimal example project for Linux that renders a colored cube to the
Oculus Quest, and requires neither Android Studio nor Gradle to build.

Instead of using Android Studio or Gradle, the project contains a build script
that uses the build tools included with the Android SDK directly.

The project source code is based on the `VrCubeWorld_NativeActivity` sample from
the Oculus Quest SDK, but heavily modified to only include the absolute minimum
functionality required to render a single cube. In particular, I've removed
support for:

* Multithreading
* Multiviews
* Multisampling
* Clamp to border textures
* Instancing

The resulting code is less than 1000 lines, and should serve as a useful
starting point for those wanting to get started with native development on the
Quest

Instead of linking separately against the `android_native_app_glue` library
(which provides the bindings between a `NativeActivity` in Java and our C code),
I've added the headers and source code for this library to the project directly.

## Prerequisites

I assume that the following packages are installed and the corresponding
environment variables point to their location:

* Android SDK (`ANDROID_HOME`)
* Android NDK (`NDK_HOME`)
* Java Runtime Environment (JRE) (`JAVA_HOME`)
* Oculus Mobile SDK (`OVR_HOME`)

In particular, I've used version 1.25.0 of the Oculus Mobile SDK. I've used
version 26 of the Android SDK, version 28.0.3 of the build tools, and version
20.0.5594570 of the NDK, because these are versions used by the samples in the
Oculus Mobile SDK.

I've used the version of the JRE that ships with the latest Android Studio. Note
that this is probably not a hard requirement. If you already have a recent
version of the JRE Environment installed, you should be able to just use that
and avoid having to install Android Studio altogether.

## Usage

I've created several shell scripts that allow you to build, install, start, and
stop the application on the Quest.

All these scripts assume that the Android SDK platform tools are in your `PATH`.
To add the Android SDK platform tools to your path, run:

export PATH=$ANDROID_HOME/platform-tools:$PATH

To build the application, run:

```build.sh```

The subsequent steps require that the Quest is connected to your machine over
USB.

To install the application, run:

```install.sh```

To start the application, run:

```./start.sh```

To stop the application, run:

```./stop.sh```
