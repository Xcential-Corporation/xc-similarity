cmake_minimum_required(VERSION 3.6.2)

project(XMLSimilar)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -o2 -pthread")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/Release)

include_directories(include
  libs
  libs/pugixml/src
  libs/rapidjson
  )

set(SOURCES
  src/Flattened2.cpp
  src/Interval.cpp
  src/xml-utils.cpp
  libs/pugixml/src/pugixml.cpp
  src/utils.cpp
  src/DocStructure.cpp
  )

add_executable(XMLSimilar src/XMLSimilarity.cpp ${SOURCES})
