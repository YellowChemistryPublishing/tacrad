cmake_minimum_required(VERSION 3.15)

project(tacrad)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/dependencies/concurrentqueue")

add_executable(tacrad
    "src/main.cpp"
)
target_include_directories(tacrad PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dependencies/concurrentqueue")
set_target_properties(tacrad PROPERTIES CXX_STANDARD 23)