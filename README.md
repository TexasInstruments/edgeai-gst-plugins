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

#Build the GStreamer plugin
PKG_CONFIG_SYSROOT_DIR=/host PKG_CONFIG_LIBDIR=/host/usr/lib/pkgconfig/ meson build --prefix=/host/usr -Dpkg_config_path=pkgconfig
ninja -C build
ninja -C build install
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/host/usr/lib/edgeai-tiovx-modules/:/host/usr/lib/aarch64-linux-gnu # This will also be needed before running the element in another terminal for example
ldconfig

# Since ninja will install these to the host you'll also need to call the following command before running a pipeline. "/host/usr/lib/aarch64-linux-gnu/gstreamer-1.0/" is not a standard GStreamer installation path
export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:/host/usr/lib/aarch64-linux-gnu/gstreamer-1.0/

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
  Version                  0.2.2
  License                  Proprietary
  Source module            GstTIOVX
  Binary package           GstTIOVX source release
  Origin URL               http://ti.com

  tiovxcolorconvert: TIOVX ColorConvert
  tiovxdlcolorblend: TIOVX DL ColorBlend
  tiovxdlpreproc: TIOVX DL PreProc
  tiovxmultiscaler: TIOVX MultiScaler
  tiovxmosaic: TIOVX Mosaic

  5 features:
  +-- 5 elements
```

| Extended Documentation |
| -----------   |
| [GstTIOVXColorConvert](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxcolorconvert)   |
| [GstTIOVXDLColorBlend](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlcolorblend)   |
| [GstTIOVXDLPreProc](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxdlpreproc)   |
| [GstTIOVXMultiScaler](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmultiscaler)   |
| [GstTIOVXMosaic](https://github.com/TexasInstruments/edgeai-gst-plugins/wiki/tiovxmosaic)   |
