find_package(blend2d QUIET)

if(NOT blend2d_FOUND)
    ExternalProject_Add(
        asmjit_external
        GIT_REPOSITORY http://git.mirror.gddi.io/mirror/asmjit.git
        GIT_TAG master
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )

    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
        set(BLEND2D_CMAKE_CXX_FLAGS "-mfpu=neon")
    endif()

    set(ASMJIT_DIR ${EXTERNAL_INSTALL_LOCATION}/src/asmjit_external)
    ExternalProject_Add(
        blend2d_external
        DEPENDS asmjit_external
        GIT_REPOSITORY http://git.mirror.gddi.io/mirror/blend2d.git
        GIT_TAG 2910c87c5e8ddedcf80bbf4871bb59d70e1170c1
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DASMJIT_DIR=${ASMJIT_DIR} -DBLEND2D_TEST=OFF -DBLEND2D_STATIC=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_CXX_FLAGS=${BLEND2D_CMAKE_CXX_FLAGS}
        BUILD_BYPRODUCTS blend2d
    )

    add_library(blend2d::blend2d STATIC IMPORTED)
    add_dependencies(blend2d::blend2d blend2d_external)
    set_target_properties(blend2d::blend2d PROPERTIES IMPORTED_LOCATION "blend2d")
endif()

file(GLOB ModuleFiles
        src/modules/cvrelate/*.c??
        src/nodes/postprocess/graphics_node_v2.cpp
        src/nodes/postprocess/report_node_v2.cpp)
add_compile_definitions(WITH_BLEND2D)
set(LibFiles "${LibFiles};${ModuleFiles}")
set(LinkLibraries "${LinkLibraries};blend2d::blend2d;")
