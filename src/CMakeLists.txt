add_executable(dexpo dexpo.cpp desktop_pixmap.cpp dexpo_socket.cpp drawable.cpp
                     window.cpp)

target_link_libraries(
  dexpo
  PRIVATE project_options project_warnings
  PUBLIC xcb)

add_executable(daemon daemon.cpp desktop_pixmap.cpp dexpo_socket.cpp
                      drawable.cpp)

target_link_libraries(
  daemon
  PRIVATE project_options project_warnings
  PUBLIC xcb xcb-randr)
