file(GLOB_RECURSE SRCS *.cpp)

include_directories(${CMAKE_SOURCE_DIR})


add_executable(examples  ${SRCS})

target_link_libraries(examples libOTe cryptoTools)