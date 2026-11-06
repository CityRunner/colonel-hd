# SPDX-License-Identifier: GPL-3.0-or-later
# The layers are a library of their own so that a CI test can check them
# on synthetic resources (tests/golden/layers_test.cpp). The cursor's
# scaling is the program's own (src/app/src/game_screen.cpp), compiled in
# as the app tests do.
add_library(sci_golden_layers STATIC layers.cpp ${PROJECT_SOURCE_DIR}/src/app/src/game_screen.cpp)
target_include_directories(sci_golden_layers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                             PRIVATE ${PROJECT_SOURCE_DIR}/src/app/src)
target_link_libraries(sci_golden_layers PUBLIC sci-core sci-view sci-text sci-render PRIVATE sci-vm)
sci_target_defaults(sci_golden_layers)

add_executable(sci-golden-resources main.cpp)
target_link_libraries(sci-golden-resources PRIVATE sci_golden_layers sci_tools_common sci-pic sci-vector sci-render
                                                   sci-platform)
sci_target_defaults(sci-golden-resources)
