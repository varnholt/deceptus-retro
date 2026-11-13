cmake_minimum_required(VERSION 3.20)
project(deceptus_retro VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(DECEPTUS_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deceptus_engine/data" CACHE PATH "authoritative Deceptus game data")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(convert_assets
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/assets.py" --import-data "${DECEPTUS_DATA_DIR}"
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/room.c"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "converting assets from the main engine checkout")

add_library(retro_engine STATIC src/engine/world.c src/engine/entities.c)
target_include_directories(retro_engine PUBLIC include)
set_target_properties(retro_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(deceptus_game STATIC src/game/deceptus.c)
target_link_libraries(deceptus_game PUBLIC retro_engine)
set_target_properties(deceptus_game PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(retro_engine PRIVATE /W4 /WX)
    target_compile_options(deceptus_game PRIVATE /W4 /WX)
else()
    target_compile_options(retro_engine PRIVATE -Wall -Wextra -Werror -pedantic)
    target_compile_options(deceptus_game PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

add_library(retro_preview SHARED src/platform/host/bridge.c assets/generated/room.c)
target_link_libraries(retro_preview PRIVATE deceptus_game)
add_dependencies(retro_preview convert_assets)

include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.c)
    target_link_libraries(engine_tests PRIVATE deceptus_game)
    add_test(NAME engine COMMAND engine_tests)
    add_test(NAME converters COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -p test_*.py)
    set_tests_properties(converters PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
