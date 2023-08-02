find_package(Boost QUIET COMPONENTS filesystem program_options)

if (Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_CONFIG} (found version \"${Boost_VERSION}\")")
else()
    string(REPLACE "/" "\\/" CMAKE_C_COMPILER_FIXED ${CMAKE_C_COMPILER})

    ExternalProject_Add(
        boost_external
        URL https://mirrors.aliyun.com/blfs/conglomeration/boost/boost_1_69_0.tar.bz2
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./bootstrap.sh --with-toolset=gcc --prefix=${EXTERNAL_INSTALL_LOCATION} &&
            /bin/sh -c "sed -i 's/using gcc/using gcc : arm : ${CMAKE_C_COMPILER_FIXED}/' project-config.jam"
        BUILD_COMMAND ./b2 --with-filesystem --with-program_options --link=static runtime-link=static cxxflags=-fPIC install
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${EXTERNAL_INSTALL_LOCATION}/lib/libboost_filesystem.a ${EXTERNAL_INSTALL_LOCATION}/lib/libboost_program_options.a
    )

    add_library(Boost::filesystem STATIC IMPORTED)
    add_dependencies(Boost::filesystem boost_external)
    set_target_properties(Boost::filesystem PROPERTIES IMPORTED_LOCATION "${EXTERNAL_INSTALL_LOCATION}/lib/libboost_filesystem.a")

    add_library(Boost::program_options STATIC IMPORTED)
    add_dependencies(Boost::program_options boost_external)
    set_target_properties(Boost::program_options PROPERTIES IMPORTED_LOCATION "${EXTERNAL_INSTALL_LOCATION}/lib/libboost_program_options.a")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(LinkLibraries "${LinkLibraries};Boost::filesystem;Boost::program_options;")