# GstTIOVX
> Repository to host GStreamer plugins for TI's EdgeAI class of devices

# Building the Project

The project can be either be built natively in the board, or cross compiled from a host PC.

## Cross Compiling the Project

```bash
# Load the PSDKR path
PSDKR_PATH=/path/to/the/ti-processor-rsdk

# Customize build for your SDK
mkdir build && cd build
../crossbuild/environment $PSDKR_PATH > aarch64-none-linux-gnu.ini

# Configure and build the project
meson .. --cross-file aarch64-none-linux-gnu.ini --cross-file ../crossbuild/crosscompile.ini
ninja
DESTDIR=$PSDKR_PATH/targetfs ninja install
```

## Compiling the Project Natively

```bash
mkdir build && cd build
meson .. --prefix=/usr -Dpkg_config_path=../pkgconfig
ninja
ninja test
sudo ninja install
```

<div style="color:gray">
    <img src="https://developer.ridgerun.com/wiki/images/2/2c/Underconstruction.png">
    This project is currently under construction.
</div>
