find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_executable(tiro_con_arco
    src/main.cpp
    src/config.cpp
    src/audio.cpp
    src/input.cpp
    src/archery_game.cpp
)

set(TIRO_LIBS)
if(TARGET SDL2::SDL2main)
    list(APPEND TIRO_LIBS SDL2::SDL2main)
endif()
list(APPEND TIRO_LIBS SDL2::SDL2)

if(TARGET SDL2::SDL2_ttf)
    list(APPEND TIRO_LIBS SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    list(APPEND TIRO_LIBS SDL2_ttf::SDL2_ttf)
else()
    list(APPEND TIRO_LIBS ${SDL2_TTF_LIBRARIES})
    target_include_directories(tiro_con_arco PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

if(TARGET SDL2_mixer::SDL2_mixer)
    list(APPEND TIRO_LIBS SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2::SDL2_mixer)
    list(APPEND TIRO_LIBS SDL2::SDL2_mixer)
else()
    list(APPEND TIRO_LIBS ${SDL2_MIXER_LIBRARIES})
    target_include_directories(tiro_con_arco PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
endif()

target_link_libraries(tiro_con_arco PRIVATE ${TIRO_LIBS})
