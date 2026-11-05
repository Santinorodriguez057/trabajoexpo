find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_image QUIET)

add_executable(menu
    src/main.cpp
    src/menu.cpp
    src/game_registry.cpp
    src/game_launcher.cpp
)

target_include_directories(menu PRIVATE src)

set(MENU_LIBS)
if(TARGET SDL2::SDL2main)
    list(APPEND MENU_LIBS SDL2::SDL2main)
endif()
list(APPEND MENU_LIBS SDL2::SDL2)

# Enlazar SDL2_ttf
if(TARGET SDL2::SDL2_ttf)
    list(APPEND MENU_LIBS SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    list(APPEND MENU_LIBS SDL2_ttf::SDL2_ttf)
else()
    list(APPEND MENU_LIBS ${SDL2_TTF_LIBRARIES})
    target_include_directories(menu PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

# Enlazar SDL2_mixer para música en bucle y efectos del menú
if(TARGET SDL2_mixer::SDL2_mixer)
    list(APPEND MENU_LIBS SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2::SDL2_mixer)
    list(APPEND MENU_LIBS SDL2::SDL2_mixer)
else()
    list(APPEND MENU_LIBS ${SDL2_MIXER_LIBRARIES})
    target_include_directories(menu PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
endif()

# Enlazar SDL2_image para carga de logos PNG con transparencia
if(TARGET SDL2_image::SDL2_image)
    list(APPEND MENU_LIBS SDL2_image::SDL2_image)
    target_compile_definitions(menu PRIVATE HAVE_SDL2_IMAGE=1)
elseif(TARGET SDL2::SDL2_image)
    list(APPEND MENU_LIBS SDL2::SDL2_image)
    target_compile_definitions(menu PRIVATE HAVE_SDL2_IMAGE=1)
elseif(SDL2_IMAGE_FOUND OR SDL2_image_FOUND)
    list(APPEND MENU_LIBS ${SDL2_IMAGE_LIBRARIES})
    target_include_directories(menu PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(menu PRIVATE HAVE_SDL2_IMAGE=1)
else()
    find_library(SDL2_IMAGE_LIB NAMES SDL2_image)
    if(SDL2_IMAGE_LIB)
        list(APPEND MENU_LIBS ${SDL2_IMAGE_LIB})
        target_compile_definitions(menu PRIVATE HAVE_SDL2_IMAGE=1)
    endif()
endif()

target_link_libraries(menu PRIVATE ${MENU_LIBS})
