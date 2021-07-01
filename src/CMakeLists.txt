set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(dexpo dexpo.cpp)

add_executable(daemon dexpo-daemon.cpp)

target_link_libraries(
  dexpo
  PRIVATE dexpo_lib project_options project_warnings
  PUBLIC xcb xcb-randr)

target_link_libraries(
  daemon
  PRIVATE dexpo_lib project_options project_warnings
  PUBLIC xcb xcb-randr)
