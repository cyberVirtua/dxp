# Define name of the binary
# add_executable(dexpo dexpo.cpp desktop_pixmap.cpp drawable.cpp)
add_executable(example example_desktop_pixmap_usage.cpp desktop_pixmap.cpp
                       drawable.cpp)

# Tells compiler to link xcb to our build
# target_link_libraries(
#   dexpo
#   PRIVATE project_options project_warnings
#   PUBLIC xcb)

# Tells compiler to link xcb to our build
target_link_libraries(
  example
  PRIVATE project_options project_warnings
  PUBLIC xcb)
