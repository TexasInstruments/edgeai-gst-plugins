# GstTIOVX
Repository to host GStreamer plugins for TI's EdgeAI class of devices

## Build the GstTIOVX with cross compilation

```
# Add the path to the cross compiler
CROSS_COMPILE=<...>
# Add the path to the targetfs/
SYSROOT_PATH=<...>

# Update the Meson's cross compile file with the cross compiler & sysroot
sed -i "s|cross_compiler=''|cross_compiler='$CROSS_COMPILE'|g" crosscompile.txt
sed -i "s|sysroot_path=''|sysroot_path='$SYSROOT_PATH'|g" crosscompile.txt

# Build in cross compilation mode
meson build --prefix=/usr --cross-file crosscompile.txt
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

