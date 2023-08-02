find_package(Eigen3 QUIET)

if (Eigen3_FOUND)
    message(STATUS "Found Eigen3: ${Eigen3_CONFIG} (found version \"${Eigen3_VERSION}\")")
    include_directories(${Eigen3_INCLUDE_DIRS}/eigen3)
    return()
endif()

ExternalProject_Add(
    Eigen3_external
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.1
    GIT_SHALLOW TRUE
    PREFIX ${EXTERNAL_INSTALL_LOCATION}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
)

add_library(Eigen3::Eigen INTERFACE IMPORTED)
add_dependencies(Eigen3::Eigen Eigen3_external)
include_directories(${EXTERNAL_INSTALL_LOCATION}/include/eigen3)