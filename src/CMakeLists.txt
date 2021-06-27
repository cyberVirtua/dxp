set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -pthread")

add_executable(dexpo dexpo.cpp desktop.cpp socket.cpp drawable.cpp xcb_util.cpp
                     window.cpp)

add_executable(daemon daemon.cpp desktop.cpp socket.cpp drawable.cpp
                      xcb_util.cpp)

target_link_libraries(
  dexpo
  PRIVATE project_options project_warnings
  PUBLIC xcb xcb-randr)

target_link_libraries(
  daemon
  PRIVATE project_options project_warnings
  PUBLIC xcb xcb-randr)
