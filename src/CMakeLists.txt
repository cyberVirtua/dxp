add_executable(daemon daemon.cpp desktop_pixmap.cpp dexpo_socket.cpp)

# Tells compiler to link xcb to our build
target_link_libraries(
  daemon
  PRIVATE project_options project_warnings
  PUBLIC xcb)
