find_package(SDL2 REQUIRED)

add_executable(template_game src/main.cpp)

target_link_libraries(template_game PRIVATE SDL2::SDL2)