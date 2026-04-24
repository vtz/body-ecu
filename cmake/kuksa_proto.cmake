# cmake/kuksa_proto.cmake — Generate C++ stubs from vendored kuksa.val.v2 proto files.
#
# Creates the `kuksa_proto` library target linking against gRPC++ and protobuf.
# Requires system-installed gRPC and Protobuf (e.g. grpc-devel, protobuf-devel on Fedora).

if(TARGET kuksa_proto)
    return()
endif()

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_PROTO_DIR "${_REPO_ROOT}/proto")
set(_GEN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/_kuksa_gen")

file(MAKE_DIRECTORY "${_GEN_DIR}/kuksa/val/v2")

set(_TYPES_PROTO "${_PROTO_DIR}/kuksa/val/v2/types.proto")
set(_VAL_PROTO   "${_PROTO_DIR}/kuksa/val/v2/val.proto")

# Locate plugin executables
get_target_property(_PROTOC protobuf::protoc IMPORTED_LOCATION)
if(NOT _PROTOC)
    find_program(_PROTOC protoc REQUIRED)
endif()

get_target_property(_GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
if(NOT _GRPC_PLUGIN)
    find_program(_GRPC_PLUGIN grpc_cpp_plugin REQUIRED)
endif()

# Generated file paths
set(_GEN_TYPES_CC "${_GEN_DIR}/kuksa/val/v2/types.pb.cc")
set(_GEN_TYPES_H  "${_GEN_DIR}/kuksa/val/v2/types.pb.h")
set(_GEN_VAL_CC   "${_GEN_DIR}/kuksa/val/v2/val.pb.cc")
set(_GEN_VAL_H    "${_GEN_DIR}/kuksa/val/v2/val.pb.h")
set(_GEN_VAL_GRPC_CC "${_GEN_DIR}/kuksa/val/v2/val.grpc.pb.cc")
set(_GEN_VAL_GRPC_H  "${_GEN_DIR}/kuksa/val/v2/val.grpc.pb.h")

add_custom_command(
    OUTPUT ${_GEN_TYPES_CC} ${_GEN_TYPES_H}
    COMMAND ${_PROTOC}
        --proto_path=${_PROTO_DIR}
        --cpp_out=${_GEN_DIR}
        ${_TYPES_PROTO}
    DEPENDS ${_TYPES_PROTO}
    COMMENT "Generating C++ from types.proto"
)

add_custom_command(
    OUTPUT ${_GEN_VAL_CC} ${_GEN_VAL_H} ${_GEN_VAL_GRPC_CC} ${_GEN_VAL_GRPC_H}
    COMMAND ${_PROTOC}
        --proto_path=${_PROTO_DIR}
        --cpp_out=${_GEN_DIR}
        --grpc_out=${_GEN_DIR}
        --plugin=protoc-gen-grpc=${_GRPC_PLUGIN}
        ${_VAL_PROTO}
    DEPENDS ${_VAL_PROTO} ${_GEN_TYPES_H}
    COMMENT "Generating C++ and gRPC from val.proto"
)

add_library(kuksa_proto STATIC
    ${_GEN_TYPES_CC}
    ${_GEN_VAL_CC}
    ${_GEN_VAL_GRPC_CC}
)
target_include_directories(kuksa_proto PUBLIC ${_GEN_DIR})
target_link_libraries(kuksa_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)
set_target_properties(kuksa_proto PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON
)
