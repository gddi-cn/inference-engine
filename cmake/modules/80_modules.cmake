file(GLOB ModuleLibFiles
    src/modules/bytetrack/*.c??
    src/modules/postprocess/*.c??)

list(APPEND ModuleLibFiles "src/modules/wrapper/tsing_jpeg_encode.cpp;src/modules/wrapper/tscv_operator.cpp")

add_library(gddi_post SHARED ${ModuleLibFiles})
set_target_properties(gddi_post PROPERTIES
    PUBLIC_HEADER "src/modules/types.hpp;src/modules/bytetrack/target_tracker.h;src/modules/postprocess/cross_border.h;")
set_target_properties(gddi_post PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_link_libraries(gddi_post PRIVATE ${OpenCV_LIBRARIES} Eigen3::Eigen spdlog::spdlog tscv mpi)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release")
    install(TARGETS gddi_post
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
endif()

set(LinkLibraries "${LinkLibraries};gddi_post")