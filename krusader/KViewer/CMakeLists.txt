set(KViewer_SRCS
    krviewer.cpp
    kimagefilepreview.cpp
    panelviewer.cpp
    diskusageviewer.cpp
    lister.cpp)

kde4_add_library(KViewer STATIC ${KViewer_SRCS})
