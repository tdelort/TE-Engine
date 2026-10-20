# find Powershell executable
find_program(POWERSHELL_PATH NAMES pwsh)

add_custom_target(
    build_engine_shaders ALL
    COMMAND ${POWERSHELL_PATH}  
        "${PROJECT_SOURCE_DIR}/tools/compile_all_engine_shaders.ps1" 
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tools/
)

add_custom_target(
    deploy_engine_shaders_dx12 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/dx12" "${CMAKE_BINARY_DIR}/data/engine/shaders/dx12"
)

# message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
# message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
# message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

add_dependencies(deploy_engine_shaders_dx12 build_engine_shaders)
