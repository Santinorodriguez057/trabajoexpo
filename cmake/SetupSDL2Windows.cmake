# cmake/SetupSDL2Windows.cmake
# Descarga y configura automaticamente SDL2, SDL2_ttf y SDL2_mixer en Windows
# para que no requiera configuracion manual del usuario.

if(NOT WIN32)
    return()
endif()

# Intentar encontrar primero en el sistema o vcpkg si ya estuvieran instaladas
find_package(SDL2 QUIET)
find_package(SDL2_ttf QUIET)
find_package(SDL2_mixer QUIET)

set(WINDOWS_DEPS_DIR "${CMAKE_SOURCE_DIR}/windows_deps")

if(NOT SDL2_FOUND OR NOT SDL2_ttf_FOUND OR NOT SDL2_mixer_FOUND)
    message(STATUS "[Windows SDL2] Dependencias no encontradas en el sistema. Configurando en ${WINDOWS_DEPS_DIR}...")
    file(MAKE_DIRECTORY "${WINDOWS_DEPS_DIR}")

    # Determinar si es MinGW o MSVC / clang-cl
    if(MINGW)
        set(SDL_FLAVOR "mingw")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDL_ARCH_TAG "x86_64-w64-mingw32")
        else()
            set(SDL_ARCH_TAG "i686-w64-mingw32")
        endif()
    else()
        set(SDL_FLAVOR "VC")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDL_ARCH_TAG "x64")
        else()
            set(SDL_ARCH_TAG "x86")
        endif()
    endif()

    set(SDL2_VERSION "2.30.8")
    set(SDL2_TTF_VERSION "2.22.0")
    set(SDL2_MIXER_VERSION "2.8.0")

    macro(download_and_extract_sdl DEP_NAME VERSION URL_PREFIX FOLDER_NAME)
        set(_ZIP_NAME "${DEP_NAME}-devel-${VERSION}-${SDL_FLAVOR}.zip")
        set(_ZIP_PATH "${WINDOWS_DEPS_DIR}/${_ZIP_NAME}")
        set(_TARGET_DIR "${WINDOWS_DEPS_DIR}/${FOLDER_NAME}-${VERSION}")

        if(NOT EXISTS "${_TARGET_DIR}")
            if(NOT EXISTS "${_ZIP_PATH}")
                set(_URL "${URL_PREFIX}/release-${VERSION}/${_ZIP_NAME}")
                message(STATUS "[Windows SDL2] Descargando ${_ZIP_NAME}...")
                file(DOWNLOAD "${_URL}" "${_ZIP_PATH}"
                     SHOW_PROGRESS
                     STATUS _DL_STATUS
                     TLS_VERIFY ON)
                list(GET _DL_STATUS 0 _STATUS_CODE)
                if(NOT _STATUS_CODE EQUAL 0)
                    message(FATAL_ERROR "[Windows SDL2] Error al descargar ${_URL}: ${_DL_STATUS}")
                endif()
            endif()

            message(STATUS "[Windows SDL2] Extrayendo ${_ZIP_NAME}...")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${_ZIP_PATH}"
                WORKING_DIRECTORY "${WINDOWS_DEPS_DIR}"
                RESULT_VARIABLE _TAR_RES
            )
            if(NOT _TAR_RES EQUAL 0)
                message(FATAL_ERROR "[Windows SDL2] Error al descomprimir ${_ZIP_PATH}")
            endif()
        endif()
    endmacro()

    download_and_extract_sdl("SDL2" "${SDL2_VERSION}"
        "https://github.com/libsdl-org/SDL/releases/download"
        "SDL2")

    download_and_extract_sdl("SDL2_ttf" "${SDL2_TTF_VERSION}"
        "https://github.com/libsdl-org/SDL_ttf/releases/download"
        "SDL2_ttf")

    download_and_extract_sdl("SDL2_mixer" "${SDL2_MIXER_VERSION}"
        "https://github.com/libsdl-org/SDL_mixer/releases/download"
        "SDL2_mixer")

    if(MINGW)
        list(APPEND CMAKE_PREFIX_PATH
            "${WINDOWS_DEPS_DIR}/SDL2-${SDL2_VERSION}/${SDL_ARCH_TAG}"
            "${WINDOWS_DEPS_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}/${SDL_ARCH_TAG}"
            "${WINDOWS_DEPS_DIR}/SDL2_mixer-${SDL2_MIXER_VERSION}/${SDL_ARCH_TAG}"
        )
    else()
        list(APPEND CMAKE_PREFIX_PATH
            "${WINDOWS_DEPS_DIR}/SDL2-${SDL2_VERSION}"
            "${WINDOWS_DEPS_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}"
            "${WINDOWS_DEPS_DIR}/SDL2_mixer-${SDL2_MIXER_VERSION}"
        )
    endif()

    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "Search path for find_package" FORCE)
endif()

# Recolectar DLLs para copiarlas junto a los ejecutables
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(EXISTS "${WINDOWS_DEPS_DIR}")
    file(GLOB_RECURSE _ALL_DLLS "${WINDOWS_DEPS_DIR}/*.dll")
    set(WINDOWS_SDL_DLLS "")
    foreach(_DLL ${_ALL_DLLS})
        # Filtrar por la arquitectura correcta para no mezclar x86 con x64
        if(_DLL MATCHES "/${SDL_ARCH_TAG}/" OR _DLL MATCHES "\\\\${SDL_ARCH_TAG}\\\\")
            list(APPEND WINDOWS_SDL_DLLS "${_DLL}")
            file(COPY "${_DLL}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        endif()
    endforeach()
endif()

# Copiar games.registry a build/bin
if(EXISTS "${CMAKE_SOURCE_DIR}/games.registry")
    file(COPY "${CMAKE_SOURCE_DIR}/games.registry" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

# Asegurar que en cada build se vuelvan a sincronizar si hubo cambios
if(WINDOWS_SDL_DLLS)
    add_custom_target(copy_windows_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WINDOWS_SDL_DLLS} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMENT "Sincronizando DLLs de SDL2 en ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()

add_custom_target(copy_registry ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/games.registry" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/games.registry"
    COMMENT "Sincronizando games.registry en ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
