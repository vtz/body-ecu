# cmake/someip_codegen.cmake — Generate C++ headers from config/services.yaml.
#
# Creates:
#   someip_codegen  — custom target that regenerates when the YAML changes
#   someip_ids      — INTERFACE library exposing the generated include dir

if(TARGET someip_ids)
    return()
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_SERVICES_YAML "${_REPO_ROOT}/config/services.yaml")
set(_GENERATOR     "${_REPO_ROOT}/scripts/generate_someip_config.py")
set(_GEN_DIR       "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_GEN_IDS_H     "${_GEN_DIR}/someip_service_ids.h")
set(_GEN_MPU_H     "${_GEN_DIR}/someip_mpu_config.h")

add_custom_command(
    OUTPUT ${_GEN_IDS_H} ${_GEN_MPU_H}
    COMMAND ${Python3_EXECUTABLE} ${_GENERATOR} ${_SERVICES_YAML} ${_GEN_DIR}
    DEPENDS ${_SERVICES_YAML} ${_GENERATOR}
    COMMENT "Generating SOME/IP headers from services.yaml"
)

add_custom_target(someip_codegen DEPENDS ${_GEN_IDS_H} ${_GEN_MPU_H})

add_library(someip_ids INTERFACE)
target_include_directories(someip_ids INTERFACE ${_GEN_DIR})
add_dependencies(someip_ids someip_codegen)
