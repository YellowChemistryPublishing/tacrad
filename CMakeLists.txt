cmake_minimum_required(VERSION 3.15)

project(tacrad)

set(FIREWORK_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/deps/Firework.Runtime")

add_custom_target(PackageTacradAssets
    COMMAND ${CMAKE_COMMAND}
    -DFIREWORK_PACKAGER_EXECUTABLE="$<TARGET_FILE:Firework.Packager>"
    -DPACKAGE_INPUT_FOLDER="${CMAKE_CURRENT_LIST_DIR}/assets"
    -DPACKAGE_OUTPUT_FILE="$<TARGET_FILE_DIR:tacrad>/Runtime/CorePackage.fwpkg"
    -P "${CMAKE_CURRENT_LIST_DIR}/deps/Firework.Runtime/cmake/PackageArchive.cmake"
)
add_dependencies(PackageTacradAssets Firework.Packager)

file(GLOB_RECURSE SRCS
    "src/*.h"
    "src/*.cpp"
)

add_executable(tacrad ${SRCS})

target_include_directories(tacrad PRIVATE "src/")
target_link_libraries(tacrad PRIVATE Firework.Components.Core2D)

set_target_properties(tacrad PROPERTIES CXX_STANDARD 23)

add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Typography>
    $<TARGET_FILE_DIR:tacrad>
)
add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Runtime.GL>
    $<TARGET_FILE_DIR:tacrad>
)
add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Runtime.RenderPipeline>
    $<TARGET_FILE_DIR:tacrad>
)
add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Runtime.CoreLib>
    $<TARGET_FILE_DIR:tacrad>
)
add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Components.Core2D>
    $<TARGET_FILE_DIR:tacrad>
)
add_custom_command(TARGET tacrad POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Firework.Components.Core3D>
    $<TARGET_FILE_DIR:tacrad>
)