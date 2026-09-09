find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(menu
    src/main.cpp
    src/menu.cpp
    src/game_registry.cpp
    src/game_launcher.cpp
)

target_include_directories(menu PRIVATE src)

# SDL2_ttf puede exportar su target con distintos nombres segun la version.
if(TARGET SDL2::SDL2_ttf)
    target_link_libraries(menu PRIVATE SDL2::SDL2 SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(menu PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
else()
    target_link_libraries(menu PRIVATE SDL2::SDL2 ${SDL2_TTF_LIBRARIES})
    target_include_directories(menu PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()