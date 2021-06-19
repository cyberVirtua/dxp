# Define name of the binary
add_executable(dexpo dexpo.cpp desktop_pixmap.cpp drawable.cpp window.cpp wmctrl.cpp)

# Tells compiler to link xcb to our build
target_link_libraries(
  dexpo
  PRIVATE project_options project_warnings
  PUBLIC xcb)
