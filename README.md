# GstTIOVX
> Repository to host GStreamer plugins for TI's EdgeAI class of devices

# Building the Project

The project can be either be built natively in the board, or cross compiled from a host PC.

## Cross Compiling the Project

```bash
# Load the PSDKR path
PSDKR_PATH=/path/to/the/ti-processor-rsdk

# Customize build for your SDK
crossbuild/environment $PSDKR_PATH > aarch64-none-linux-gnu.ini

# Configure and build the project
meson build --cross-file aarch64-none-linux-gnu.ini --cross-file crossbuild/crosscompile.ini
ninja -C build -j8
DESTDIR=$PSDKR_PATH/targetfs ninja install
```

## Compiling the Project Natively

```bash
meson build --prefix=/usr -Dpkg_config_path=pkgconfig
ninja -C build -j4
ninja -C build test
sudo ninja -C build install
```

# GStreamer elements
```bash
root@j7-2:~# gst-inspect-1.0 tiovx
Plugin Details:
  Name                     tiovx
  Description              GStreamer plugin for TIOVX
  Filename                 /usr/lib/gstreamer-1.0/libgsttiovx.so
  Version                  0.0.1
  License                  Proprietary
  Source module            GstTIOVX
  Binary package           GstTIOVX source release
  Origin URL               http://ti.com

  tiovxcolorconvert: TIOVX ColorConvert
  tiovxmultiscaler: TIOVX MultiScaler

  2 features:
  +-- 2 elements
```

| Extended Documentation |
| -----------   |
| [GstTIOVXColorConvert](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxcolorconvert)   |
| [GstTIOVXMultiScaler](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmultiscaler)   |
