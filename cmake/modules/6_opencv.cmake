find_package(OpenCV QUIET)

if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_CONFIG} (found version \"${OpenCV_VERSION}\")")
    file(GLOB ModuleFiles
        deps/yuv_rgb/yuv_rgb.c
        src/OpenCV/cv_tools.cpp

        # src/OpenCV/cv_common.cpp
        # src/modules/tracking/*.c??
        src/modules/report_result.cpp
        src/modules/cvrelate/graphics.cpp
        src/nodes/target_tracker_node_v2.cpp
    )
    add_compile_definitions(WITH_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})
    set(LibFiles "${LibFiles};${ModuleFiles}")
    set(LinkLibraries "${LinkLibraries};${OpenCV_LIBRARIES}")
else()
    message(STATUS "OpenCV not Found, feature with OpenCV will closed!")
endif()