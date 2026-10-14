find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(test_mando src/main.cpp)

set(TEST_MANDO_LIBS SDL2::SDL2)
if(TARGET SDL2::SDL2main)
    list(APPEND TEST_MANDO_LIBS SDL2::SDL2main)
endif()

if(TARGET SDL2::SDL2_ttf)
    list(APPEND TEST_MANDO_LIBS SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    list(APPEND TEST_MANDO_LIBS SDL2_ttf::SDL2_ttf)
else()
    list(APPEND TEST_MANDO_LIBS ${SDL2_TTF_LIBRARIES})
    target_include_directories(test_mando PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

target_link_libraries(test_mando PRIVATE ${TEST_MANDO_LIBS})