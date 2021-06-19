cmake_minimum_required(VERSION 3.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(extract-xiso)

string(TOUPPER __${CMAKE_SYSTEM_NAME}__ TARGET_OS)

set(SOURCE_FILES
  main.cpp
  exiso_xiso.cpp
  exiso_avl.cpp
  exiso_log.cpp
  boyer_moore.cpp
)

add_executable(extract-xiso ${SOURCE_FILES})
target_compile_definitions(extract-xiso PRIVATE ${TARGET_OS})
install(TARGETS extract-xiso RUNTIME DESTINATION "${INSTALL_PREFIX}/bin")
