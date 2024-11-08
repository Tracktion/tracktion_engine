function (addModuleSourceTarget)
    if (TARGET Modules)
        return()
    endif()

    file(GLOB_RECURSE ModuleFiles CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/juce/modules/*.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/juce/modules/*.h
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/3rd_party/choc/*.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/3rd_party/choc/*.h
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_engine/*.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_engine/*.h
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_graph/*.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_graph/*.h
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_core/*.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/tracktion_core/*.h)
    source_group(TREE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules PREFIX Modules FILES ${ModuleFiles})
    add_library(Modules INTERFACE ${ModuleFiles})
endfunction()