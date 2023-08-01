find_package(spdlog QUIET)

if (spdlog_FOUND)
    message(STATUS "Found spdlog: ${spdlog_CONFIG} (found version \"${spdlog_VERSION}\")")
    return()
endif()

ExternalProject_Add(
    fmt_external
    GIT_REPOSITORY http://git.mirror.gddi.io/mirror/fmt.git
    GIT_TAG 10.0.0
    GIT_SHALLOW TRUE
    PREFIX ${EXTERNAL_INSTALL_LOCATION}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_BYPRODUCTS fmt
)

ExternalProject_Add(
    spdlog_external
    DEPENDS fmt_external
    GIT_REPOSITORY http://git.mirror.gddi.io/mirror/spdlog.git
    GIT_TAG v1.9.2
    GIT_SHALLOW TRUE
    PREFIX ${EXTERNAL_INSTALL_LOCATION}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_BYPRODUCTS spdlog
)

add_library(spdlog::spdlog STATIC IMPORTED)
add_dependencies(spdlog::spdlog spdlog_external)
set_target_properties(spdlog::spdlog PROPERTIES IMPORTED_LOCATION "spdlog" INTERFACE_LINK_LIBRARIES "fmt")
set(LinkLibraries "${LinkLibraries};spdlog::spdlog")