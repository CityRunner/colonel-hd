# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(sci-review-sheet main.cpp)
target_link_libraries(sci-review-sheet PRIVATE sci_tools_common sci-platform)
sci_target_defaults(sci-review-sheet)
