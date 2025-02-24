cmake_minimum_required(VERSION 3.21)

function(load_wxwidgets)
    find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base adv aui ribbon richtext)
    target_link_libraries(${PROJECT_NAME} PRIVATE wx::core wx::base wx::adv wx::aui wx::ribbon wx::richtext)
endfunction()

function(load_sqlite3)
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3)
endfunction()

function(load_hunspell)
    set(HUNSPELL_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include/hunspell")

    if(WIN32)
        file(GLOB HUNSPELL_RELEASE_LIBS "${VCPKG_INSTALLED_DIR}/lib/hunspell*.lib")
        file(GLOB HUNSPELL_DEBUG_LIBS "${VCPKG_INSTALLED_DIR}/debug/lib/hunspell*.lib")
    elseif(UNIX)
        file(GLOB HUNSPELL_RELEASE_LIBS "${VCPKG_INSTALLED_DIR}/lib/libhunspell*.a" "${VCPKG_INSTALLED_DIR}/lib/libhunspell*.so")
        file(GLOB HUNSPELL_DEBUG_LIBS "${VCPKG_INSTALLED_DIR}/debug/lib/libhunspell*.a" "${VCPKG_INSTALLED_DIR}/debug/lib/libhunspell*.so")
    endif()

    if(HUNSPELL_RELEASE_LIBS)
        list(GET HUNSPELL_RELEASE_LIBS 0 HUNSPELL_LIBRARY_RELEASE)
    endif()

    if(HUNSPELL_DEBUG_LIBS)
        list(GET HUNSPELL_DEBUG_LIBS 0 HUNSPELL_LIBRARY_DEBUG)
    else()
        set(HUNSPELL_LIBRARY_DEBUG ${HUNSPELL_LIBRARY_RELEASE})
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE "${VCPKG_INSTALLED_DIR}/include")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:${HUNSPELL_LIBRARY_RELEASE}>"
        "$<$<CONFIG:Debug>:${HUNSPELL_LIBRARY_DEBUG}>"
        )
endfunction()