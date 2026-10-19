find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

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

if(TARGET SDL2::SDL2_ttf)
    list(APPEND MENU_LIBS SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    list(APPEND MENU_LIBS SDL2_ttf::SDL2_ttf)
else()
    list(APPEND MENU_LIBS ${SDL2_TTF_LIBRARIES})
    target_include_directories(menu PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

target_link_libraries(menu PRIVATE ${MENU_LIBS})