cmake_minimum_required(VERSION 3.0.0)

project(threadpoll CXX C)

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

include_directories(${PROJECT_SOURCE_DIR}/inc)

add_executable(${PROJECT_NAME} ${SRC_LIST})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

target_link_libraries(${PROJECT_NAME} pthread)



