find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(test_mando src/main.cpp)

if(TARGET SDL2::SDL2_ttf)
    target_link_libraries(test_mando PRIVATE SDL2::SDL2 SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(test_mando PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
else()
    target_link_libraries(test_mando PRIVATE SDL2::SDL2 ${SDL2_TTF_LIBRARIES})
    target_include_directories(test_mando PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()