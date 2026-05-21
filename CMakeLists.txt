# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Default: Xiao Wi-Fi HaLow Carrier (MM8108, 3-0067).
# Override for other boards via SDKCONFIG_DEFAULTS env var, e.g.:
#   SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.defaults.seeed_halow_xiao_FGH100M_H" \
#       idf.py set-target esp32s3
if(DEFINED ENV{SDKCONFIG_DEFAULTS})
    set(SDKCONFIG_DEFAULTS $ENV{SDKCONFIG_DEFAULTS})
else()
    set(SDKCONFIG_DEFAULTS
        "sdkconfig.defaults"
        "sdkconfig.defaults.${IDF_TARGET}"
        "sdkconfig.defaults.halow_xiao_mm8108"
    )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(snapclient)

# Bridge for upstream morsemicro/halow 2.10.4-esp32-1 packaging bug:
# components/shims/CMakeLists.txt references idf::firmware directly, but
# the registry firmware component is registered as morsemicro__firmware
# (target idf::morsemicro__firmware), so idf::firmware never exists.
if(TARGET __idf_morsemicro__firmware AND NOT TARGET idf::firmware)
    add_library(idf::firmware ALIAS __idf_morsemicro__firmware)
endif()

# Registry tarball for morsemicro/halow strips Unix exec bits; restore them so
# scripts invoked directly from CMake custom_commands (e.g. librarymangler.py) work.
file(GLOB_RECURSE _halow_scripts
    "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/morsemicro__halow/*.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/morsemicro__halow/*.sh")
foreach(_script ${_halow_scripts})
    execute_process(COMMAND chmod +x "${_script}")
endforeach()

# Required by the HaLow netif driver for link state callbacks in LWIP.
add_compile_definitions(LWIP_NETIF_LINK_CALLBACK=1)

# this is needed by udp_logger component. We don't want autostart.
IF (CONFIG_ESP32_UDP_LOGGER_ENABLED)
idf_build_set_property(COMPILE_OPTIONS "-DESP32_UDP_LOGGER_NO_AUTOSTART" APPEND)
endif()