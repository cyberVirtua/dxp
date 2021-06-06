# Define name of the binary
add_executable(dexpo dexpo.cpp DesktopPixmap.cpp)
add_executable(example example_DesktopPixmap_usage.cpp DesktopPixmap.cpp)

# Tells compiler to link xcb to our build
target_link_libraries(
  dexpo
  PRIVATE project_options project_warnings
  PUBLIC xcb)

# Tells compiler to link xcb to our build
target_link_libraries(
  example
  PRIVATE project_options project_warnings
  PUBLIC xcb)
