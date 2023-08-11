if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavfilter libavformat libavutil libswscale libswresample)
else()
    find_package(FFMPEG REQUIRED)
endif()

message(STATUS "Found FFmpeg: ${FFMPEG_CONFIG} (found version \"${FFMPEG_VERSION}\")")
add_compile_definitions(WITH_FFMPEG)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

file(GLOB codeFile src/modules/endecode/*.cpp src/nodes/ffvcodec/*.c??)
set(LibFiles "${LibFiles};${codeFile}")

file(GLOB ModuleLibFiles src/modules/codec/*.cpp)
add_library(gddi_codec SHARED ${ModuleLibFiles})
set_target_properties(gddi_codec PROPERTIES PUBLIC_HEADER "src/modules/codec/demux_stream_v3.h;src/modules/codec/decode_video_v3.h")
set_target_properties(gddi_codec PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_link_libraries(gddi_codec PRIVATE ${FFMPEG_LIBRARIES} spdlog::spdlog Boost::filesystem)

set(LinkLibraries "${LinkLibraries};gddi_codec;${FFMPEG_LIBRARIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release")
    install(TARGETS gddi_codec
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
endif()