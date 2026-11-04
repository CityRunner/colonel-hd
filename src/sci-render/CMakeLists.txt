# SPDX-License-Identifier: GPL-3.0-or-later
sci_add_library(NAME sci-render
    SOURCES
        src/acceptance.cpp
        src/compose.cpp
        src/integrity.cpp
        src/original.cpp
        src/rasterize.cpp
        src/reference.cpp
        src/round_trip.cpp
        src/smoothing.cpp
    DEPENDS sci-core sci-pic sci-vector sci-view)
