// The one place the version lives. Used by the code and by res/app.rc (VERSIONINFO),
// so it must stay rc-compatible: #defines only.
#pragma once

#define SIDETONE_VERSION_MAJOR 1
#define SIDETONE_VERSION_MINOR 0
#define SIDETONE_VERSION_PATCH 0

#define SIDETONE_STRINGIFY_(x) #x
#define SIDETONE_STRINGIFY(x) SIDETONE_STRINGIFY_(x)
#define SIDETONE_VERSION_STRING                                                         \
    SIDETONE_STRINGIFY(SIDETONE_VERSION_MAJOR) "." SIDETONE_STRINGIFY(SIDETONE_VERSION_MINOR) "." \
    SIDETONE_STRINGIFY(SIDETONE_VERSION_PATCH)
