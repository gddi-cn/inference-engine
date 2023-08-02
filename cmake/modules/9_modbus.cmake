pkg_check_modules(modbus QUIET libmodbus)

if (modbus_FOUND)
    message(STATUS "Found modbus: ${modbus_CONFIG} (found version \"${modbus_VERSION}\")")
else()
    set(MODBUS_DIR ${EXTERNAL_INSTALL_LOCATION}/src/libmodbus_external)
    ExternalProject_Add(
        libmodbus_external
        GIT_REPOSITORY https://github.com/stephane/libmodbus.git
        GIT_TAG v3.1.10
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./autogen.sh
        BUILD_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} ./configure --srcdir=${MODBUS_DIR} --prefix=${EXTERNAL_INSTALL_LOCATION} --enable-static=yes --disable-shared --host=${CMAKE_SYSTEM_PROCESSOR}
        BUILD_BYPRODUCTS ${EXTERNAL_INSTALL_LOCATION}/lib/libmodbus.a
    )

    add_library(modbus STATIC IMPORTED)
    add_dependencies(modbus libmodbus_external)
    set_target_properties(modbus PROPERTIES IMPORTED_LOCATION "${EXTERNAL_INSTALL_LOCATION}/lib/libmodbus.a")
endif()

add_compile_definitions(WITH_MODBUS)
file(GLOB ModuleFiles src/modules/modbus/*.cpp)
set(LibFiles "${LibFiles};${ModuleFiles}")

include_directories(${EXTERNAL_INSTALL_LOCATION}/include/modbus)
set(LinkLibraries "${LinkLibraries};modbus")