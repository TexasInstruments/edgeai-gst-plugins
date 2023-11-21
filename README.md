# TI GStreamer Plugins
> Repository to host GStreamer plugins for TI's EdgeAI class of devices

# Building the Project

The project can be either be built natively in the board, or cross compiled from a host PC.

## Cross Compiling the Project

```bash
# Load the PSDKR path
PSDKR_PATH=/path/to/the/ti-processor-rsdk

# Customize build for your SDK
PKG_CONFIG_PATH='' crossbuild/environment $PSDKR_PATH > aarch64-none-linux-gnu.ini

# Configure and build the project
PKG_CONFIG_PATH='' meson build --cross-file aarch64-none-linux-gnu.ini --cross-file crossbuild/crosscompile.ini
ninja -C build
DESTDIR=$PSDKR_PATH/targetfs ninja install
```

## Compiling the Project Natively

```bash
meson build --prefix=/usr -Dpkg_config_path=pkgconfig
ninja -C build
ninja -C build test
ninja -C build install
ldconfig
```
You can delete the GStreamer registry cache if the new elements are not found
using gst-inspect and build a new one

```bash
rm -rf ~/.cache/gstreamer-1.0/registry.aarch64.bin
gst-inspect-1.0
```

## Compiling the Project Natively inside the PSDK RTOS Docker container
```bash
#Build Docker container
cd /opt/edgeai-gst-apps/docker
./docker_build.sh
./docker_run.sh
cd /opt/edgeai-gst-apps
./docker_run.sh

#Build the tiovx modules
cd /opt/edgeai-tiovx-modules/build
cmake ..
make -j$(nproc)
make install

#Build the GStreamer plugin (same as native build)
meson build --prefix=/usr -Dpkg_config_path=pkgconfig
ninja -C build
ninja -C build install

#Test plugin is loaded correctly
gst-inspect-1.0 tiovx
gst-inspect-1.0 ti
```

# GStreamer plugins
## GstTIOVX

These are plugins which internally uses TIOVX (TI's implementation of OpenVX).
These plugins are compiled for devices which have OpenVX support.
Eg: TDAV4M,AM68A,AM69A,AM62A etc.

```bash
root@tda4vm-sk:~# gst-inspect-1.0 tiovx
Plugin Details:
  Name                     tiovx
  Description              GStreamer plugin for TIOVX
  Filename                 /usr/lib/gstreamer-1.0/libgsttiovx.so
  Version                  0.7.0
  License                  Proprietary
  Source module            GstTIOVX
  Binary package           GstTIOVX source release
  Origin URL               http://ti.com

  tiovxsdeviz: TIOVX SdeViz
  tiovxsde: TIOVX Sde
  tiovxdofviz: TIOVX DofViz
  tiovxdof: TIOVX DOF
  tiovxcolorconvert: TIOVX ColorConvert
  tiovxdlcolorblend: TIOVX DL ColorBlend
  tiovxdlcolorconvert: TIOVX DL ColorConvert
  tiovxmemalloc: TIOVX Mem Alloc
  tiovxdelay: TIOVX Delay
  tiovxpyramid: TIOVX Pyramid
  tiovxmux: TIOVX Mux
  tiovxmultiscaler: TIOVX MultiScaler
  tiovxmosaic: TIOVX Mosaic
  tiovxldc: TIOVX LDC
  tiovxisp: TIOVX ISP
  tiovxdemux: TIOVX Demux

  16 features:
  +-- 16 elements
```

| Extended Documentation |
| -----------   |
| [GstTIOVXDLColorConvert](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlcolorconvert)   |
| [GstTIOVXDLPreProc](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlpreproc)   |
| [GstTIOVXMultiScaler](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmultiscaler)   |
| [GstTIOVXMosaic](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmosaic)   |
| [GstTIOVXISP](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxisp)   |
| [GstTIOVXLDC](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxldc)   |

## GstTI

These are the arm-only plugins which internally uses Arm NEON optimized kernels.
This plugins can be built on any device with gstreamer support. Eg: AM62X, AM62P.

```bash
root@tda4vm-sk:~# gst-inspect-1.0 ti
Plugin Details:
  Name                     ti
  Description              GStreamer plugin for TI devices
  Filename                 /usr/lib/gstreamer-1.0/libgstti.so
  Version                  0.7.0
  License                  Proprietary
  Source module            GstTIOVX
  Binary package           GstTIOVX source release
  Origin URL               http://ti.com

  ticolorconvert: TI Color Convert
  tidlinferer: TI DL Inferer
  tidlpostproc: TI DL PostProc
  tidlpreproc: TI DL PreProc
  timosaic: TI Mosaic
  tiperfoverlay: TI Perf Overlay
  tiscaler: TI Scaler

  7 features:
  +-- 7 elements
```

| Extended Documentation |
| -----------   |
| [GstTIColorConvert](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/ticolorconvert)   |
| [GstTIDLInferer](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tidlinferer)   |
| [GstTIDLPostProc](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tidlpostproc)   |
| [GstTIDLPreProc](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tidlpreproc)   |
| [GstTIMosaic](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/timosaic)   |
| [GstTIPerfOverlay](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiperfoverlay)   |
| [GstTIScaler](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiscaler)   |