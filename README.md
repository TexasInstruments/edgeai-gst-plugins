# GstTIOVX
Repository to host GStreamer plugins for TI's EdgeAI class of devices

## Cross compiling the project

```
# Load the PSDKR path
PSDKR_PATH=<...>

# Update the Meson's cross compile file with the PSDKR path
sed -i "s|PSDKR=|PSDKR='$PSDKR_PATH'|g" crossbuild/aarch64.ini

# Build in cross compilation mode
meson build --prefix=/usr --cross-file aarch64.ini --cross-file crosscompile.txt
ninja -C build/ -j8
```

## Installing GstTIOVX

```
meson build --prefix=/usr
ninja -C build/ -j8
ninja -C build/ install
```

<div style="color:gray">
    <img src="https://developer.ridgerun.com/wiki/images/2/2c/Underconstruction.png">
    This project is currently under construction.
</div>

