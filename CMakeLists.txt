cmake_minimum_required (VERSION 3.12)

project (WindowHandler)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_TESTS "Build tests" OFF)


# Add library and source

add_library (MWindow) # can be made STATIC or SHARED in presets (BUILD_SHARED_LIBS var)

if(MSVC)
    target_compile_options(MWindow PRIVATE 
        /MP
        /utf-8
    )
    target_compile_definitions(MWindow PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(MWindow PRIVATE /showIncludes)
endif()


if(WIN32)
    target_compile_definitions(MWindow PUBLIC MPLATFORM_WINDOWS)
    target_sources(MWindow
        PRIVATE

        src/platform/windows/MWindowImpl.cpp
        src/platform/windows/MGlobalImpl.cpp
    )
    target_link_libraries(MWindow PRIVATE Shcore dxgi.lib dwmapi.lib)
elseif(ANDROID)
    target_link_options(MWindow INTERFACE "-Wl,--export-dynamic")

    target_compile_definitions(MWindow PUBLIC MPLATFORM_ANDROID)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS App Store forbids full symbol export
    target_compile_definitions(MWindow PUBLIC MPLATFORM_IOS)  
elseif(APPLE) # macOS — must come after iOS check

    target_link_options(MWindow INTERFACE "-rdynamic")
    
    target_compile_definitions(MWindow PUBLIC MPLATFORM_MAC)
elseif(UNIX) # Linux
    target_link_options(MWindow INTERFACE "-Wl,--export-dynamic")

    target_compile_definitions(MWindow PUBLIC MPLATFORM_LINUX)
endif()

target_sources(MWindow
    PRIVATE

    src/MWindow.cpp
    
) 

target_include_directories(MWindow
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

include(GNUInstallDirs)

install(TARGETS MWindow
    EXPORT MWindowTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


install(EXPORT MWindowTargets
    FILE MWindowTargets.cmake
    NAMESPACE MWindow::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWindow
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/MWindowConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MWindowConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWindow
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/MWindowConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWindow
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()