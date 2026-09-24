# MSVC toolchain file for Ninja generator
# Forces use of MSVC compiler and linker to avoid conflicts with MinGW

set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)
set(CMAKE_LINKER link)

# Qt is resolved by cmake/SideloadQt.cmake (project-local, pinned copy) unless
# VELOCITY_SIDELOAD_QT=OFF, in which case set CMAKE_PREFIX_PATH yourself.
