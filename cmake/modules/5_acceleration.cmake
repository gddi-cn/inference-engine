file(GLOB ModuleFiles src/modules/algorithm/tsing_*.cpp)
set(LinkLibraries "${LinkLibraries};gddeploy_app;gddeploy_api;gddeploy_core;gddeploy_register;gddeploy_common;mpi")
set(SDK_DOWNLOAD_URL "http://cacher.devops.io/api/cacher/files/86709875d83868e48ba415719d4b17d9cc1f15cf537b8f229a67432967ddad0e")
set(SDK_URL_HASH "86709875d83868e48ba415719d4b17d9cc1f15cf537b8f229a67432967ddad0e")


include(FetchContent)
FetchContent_Declare(
    inference-sdk
    URL ${SDK_DOWNLOAD_URL}
    URL_HASH SHA256=${SDK_URL_HASH}
)

FetchContent_MakeAvailable(inference-sdk)
include_directories(${inference-sdk_SOURCE_DIR}/include)
link_directories(${inference-sdk_SOURCE_DIR}/lib)

file(GLOB NodeFiles src/nodes/algorithm/*.cpp
                    src/modules/wrapper/draw_image.cpp
                    src/modules/wrapper/export_video.cpp)
set(LibFiles "${LibFiles};${ModuleFiles};${NodeFiles};${CuFiles}")