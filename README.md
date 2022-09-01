# GstTIOVX
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
cd /opt/edge_ai_apps/docker
./docker_build.sh
./docker_run.sh
cd /opt/edge_ai_apps
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
```

# GStreamer elements
```bash
root@j7-2:~# gst-inspect-1.0 tiovx
Plugin Details:
  Name                     tiovx
  Description              GStreamer plugin for TIOVX
  Filename                 /usr/lib/gstreamer-1.0/libgsttiovx.so
  Version                  0.7.0
  License                  Proprietary
  Source module            GstTIOVX
  Binary package           GstTIOVX source release
  Origin URL               http://ti.com

  tiovxdemux: TIOVX Demux
  tiovxmux: TIOVX Mux
  tiovxmultiscaler: TIOVX MultiScaler
  tiovxmosaic: TIOVX Mosaic
  tiovxldc: TIOVX LDC
  tiovxisp: TIOVX ISP
  tiovxdlpreproc: TIOVX DL PreProc
  tiovxdlcolorblend: TIOVX DL ColorBlend
  tiovxcolorconvert: TIOVX ColorConvert

  9 features:
  +-- 9 elements

```

| Extended Documentation |
| -----------   |
| [GstTIOVXColorConvert](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxcolorconvert)   |
| [GstTIOVXDLColorBlend](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlcolorblend)   |
| [GstTIOVXDLPreProc](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlpreproc)   |
| [GstTIOVXISP](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxisp)   |
| [GstTIOVXLDC](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxldc)   |
| [GstTIOVXMultiScaler](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmultiscaler)   |
| [GstTIOVXMosaic](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmosaic)   |
