file(GLOB COMMON_BASIC_SRC
    src/common_basic/*.c??
    src/nodes/postprocess/*.c??)

if(NOT ${TARGET_CHIP} STREQUAL "rv1126")
list(REMOVE_ITEM COMMON_BASIC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/nodes/postprocess/conn_detector_node_v2.cpp")
list(REMOVE_ITEM COMMON_BASIC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/nodes/postprocess/vehicle_license_spliter_node_v2.cpp")
list(REMOVE_ITEM COMMON_BASIC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/nodes/postprocess/vehicle_license_parser_node_v2.cpp")
    
endif()
set(LibFiles "${LibFiles};${COMMON_BASIC_SRC}")