cmake_minimum_required(VERSION 3.16)
project(chebrium-qtstyle LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# the style: QT_STYLE_OVERRIDE=chebrium with this directory's parent on QT_PLUGIN_PATH
# Inter is inside the plugin (the same files the shell draws its text with), so a Flatpak program needs nothing installed
add_library(chebrium MODULE chebrium-style.cpp fonts.qrc)
target_link_libraries(chebrium PRIVATE Qt6::Widgets)
set_target_properties(chebrium PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/styles)
install(TARGETS chebrium LIBRARY DESTINATION lib/qt-6/plugins/styles)

# a window of widgets to look at (cmake --build . --target demo)
add_executable(demo EXCLUDE_FROM_ALL demo.cpp)
target_link_libraries(demo PRIVATE Qt6::Widgets)

# sliders for the glass of popups (writes ~/.config/memrium/glass.toml, the style follows it live); run through tools/chebrium-tweak.sh
add_executable(chebrium-tweak tweak.cpp)
target_link_libraries(chebrium-tweak PRIVATE Qt6::Widgets)

# a bigger window of widgets (the parts of a program besides buttons and menus), to look at (cmake --build . --target more)
add_executable(more EXCLUDE_FROM_ALL more.cpp)
target_link_libraries(more PRIVATE Qt6::Widgets)
