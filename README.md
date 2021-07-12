# GstTIOVX
Repository to host GStreamer plugins for TI's EdgeAI class of devices

## Cross compiling the project

```
# Load the PSDKR path
PSDKR_PATH=<...>

# Create an environment with the:
* Source path
* Build path
* PSDKR path

mkdir build && cd build
source ../crossbuild/environment  $PWD/.. $PWD  $PSDKR_PATH

# Build in cross compilation mode
meson .. --prefix=$PWD/deploy/gst-tiovx --cross-file crossbuild/aarch64.ini --cross-file ../crossbuild/crosscompile.txt
ninja
ninja install

# Deploy the binaries
ninja debfile
dpkg -x deploy/gst-tiovx.deb $PSDKR_PATH/targetfs/usr/

```

## Compiling the project natively

```
mkdir build && cd build
meson .. --prefix=/usr -Dpkg_config_path=../pkgconfig
ninja
ninja install
```

<div style="color:gray">
    <img src="https://developer.ridgerun.com/wiki/images/2/2c/Underconstruction.png">
    This project is currently under construction.
</div>

