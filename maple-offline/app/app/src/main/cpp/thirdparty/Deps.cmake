include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
        stb
        GIT_REPOSITORY    https://github.com/nothings/stb.git
        GIT_TAG           b42009b3b9d4ca35bc703f5310eedc74f584be58
        PREFIX            "${CMAKE_SOURCE_DIR}/thirdparty"
        SOURCE_DIR        "${CMAKE_SOURCE_DIR}/thirdparty/stb"
)

# glfm is vendored (brackeen/glfm @ 607fc6f, zlib license) with maple-offline controller patch.
FetchContent_MakeAvailable(stb)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/glfm)