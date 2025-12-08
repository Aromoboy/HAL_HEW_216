# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-src")
  file(MAKE_DIRECTORY "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-src")
endif()
file(MAKE_DIRECTORY
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-build"
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix"
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/tmp"
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/src/enkits-populate-stamp"
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/src"
  "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/src/enkits-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/src/enkits-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/HEW/HAL_HEW_216/HEWGame/box2d/build/_deps/enkits-subbuild/enkits-populate-prefix/src/enkits-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
