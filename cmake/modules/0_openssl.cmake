pkg_check_modules(openssl QUIET libcrypto=1.1.1n libssl=1.1.1n)

if (openssl_FOUND)
    message(STATUS "Found openssl: ${openssl_CONFIG} (found version \"${openssl_VERSION}\")")
    return()
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME)
ExternalProject_Add(
    openssl_external
    GIT_REPOSITORY http://git.mirror.gddi.io/mirror/openssl.git
    GIT_TAG OpenSSL_1_1_1n
    PREFIX ${EXTERNAL_INSTALL_LOCATION}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
        ./Configure ${SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR} --prefix=${EXTERNAL_INSTALL_LOCATION}
    BUILD_BYPRODUCTS crypto ssl
)

add_library(crypto STATIC IMPORTED)
add_dependencies(crypto openssl_external)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION "crypto")
add_library(ssl STATIC IMPORTED)
add_dependencies(ssl openssl_external)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION "ssl")

set(LinkLibraries "${LinkLibraries};crypto;ssl")