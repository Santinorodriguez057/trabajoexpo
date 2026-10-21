find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_executable(carrera_turbo
    src/main.cpp
    src/input.cpp
    src/car.cpp
    src/road.cpp
    src/renderer.cpp
    src/ui.cpp
    src/sound.cpp
)

set(CARRERA_LIBS SDL2::SDL2)
if(TARGET SDL2::SDL2main)
    list(APPEND CARRERA_LIBS SDL2::SDL2main)
endif()

if(TARGET SDL2::SDL2_ttf)
    list(APPEND CARRERA_LIBS SDL2::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf)
    list(APPEND CARRERA_LIBS SDL2_ttf::SDL2_ttf)
else()
    list(APPEND CARRERA_LIBS ${SDL2_TTF_LIBRARIES})
    target_include_directories(carrera_turbo PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
endif()

if(TARGET SDL2_mixer::SDL2_mixer)
    list(APPEND CARRERA_LIBS SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2::SDL2_mixer)
    list(APPEND CARRERA_LIBS SDL2::SDL2_mixer)
else()
    list(APPEND CARRERA_LIBS ${SDL2_MIXER_LIBRARIES})
    target_include_directories(carrera_turbo PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
endif()

target_link_libraries(carrera_turbo PRIVATE ${CARRERA_LIBS})
