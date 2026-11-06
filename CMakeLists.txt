cmake_minimum_required(VERSION 3.16)

# Имя проекта и стандарт C++
project(GameA LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Экспорт compile_commands.json для автодополнения в редакторах
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Поиск зависимостей ---

# Vulkan
find_package(Vulkan REQUIRED)

# Укажите путь к папке с исходниками GLFW (без /CMake)
set(GLFW_SOURCE_DIR "C:/Prog/Libraries/glfw-3.4")

# Подключаем GLFW как подпроект
add_subdirectory(${GLFW_SOURCE_DIR} ${CMAKE_BINARY_DIR}/glfw)

# --- Tracy Profiler ---
set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy profiling")
set(TRACY_ON_DEMAND ON CACHE BOOL "Start profiling only when server connects")
add_subdirectory(3rdparty/tracy)
add_compile_definitions(TRACY_ENABLE)


# --- Компиляция шейдеров в SPIR-V ---

# Ищем glslc в Vulkan SDK
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc не найден. Проверьте, что установлен Vulkan SDK и переменная VULKAN_SDK.")
endif()

# Функция для компиляции одного шейдера
function(compile_shader SHADER_SOURCE)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    get_filename_component(SHADER_EXT  ${SHADER_SOURCE} EXT)

    if(SHADER_EXT STREQUAL ".vert")
        set(SPV_EXT ".vert.spv")
    elseif(SHADER_EXT STREQUAL ".frag")
        set(SPV_EXT ".frag.spv")
    else()
        set(SPV_EXT "${SHADER_EXT}.spv")
    endif()

    # Меняем путь: кладём .spv прямо туда, где будет лежать .exe
    set(SPV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${SHADER_NAME}${SPV_EXT}")

    add_custom_command(
        OUTPUT  ${SPV_OUTPUT}
        COMMAND ${GLSLC_EXECUTABLE}
                --target-env=vulkan1.0
                ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}
                -o ${SPV_OUTPUT}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}
        COMMENT "Компиляция шейдера: ${SHADER_SOURCE}"
        VERBATIM
    )

    # Возвращаем сгенерированный файл в родительскую область
    set(SHADER_SPV_FILES ${SHADER_SPV_FILES} ${SPV_OUTPUT} PARENT_SCOPE)
endfunction()

# Список всех шейдеров проекта
set(SHADER_SOURCES
    shaders/CircleShader.vert
    shaders/CircleShader.frag
    shaders/QuadShader.vert
    shaders/QuadShader.frag
)

set(SHADER_SPV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
    compile_shader(${SHADER})
endforeach()

# --- Сборка исполняемого файла ---

add_executable(GameA
    GLFWHandler.cpp
    MainWindow.cpp
    VulkanCreation.cpp
    MainWindow.h
    Primitives.h
    ${SHADER_SPV_FILES}
)

# --- Подключение зависимостей ---

target_link_libraries(GameA PRIVATE Tracy::TracyClient)
target_link_libraries(GameA PRIVATE
    Vulkan::Vulkan
    glfw
)
target_include_directories(GameA PRIVATE
    "C:/Program Files/Microsoft Visual Studio/18/Community/Libraries/glm"
)

# --- Настройки для MSVC ---

if(MSVC)
    target_compile_options(GameA PRIVATE /W4 /permissive-)
    # Многопоточная статическая или динамическая CRT — выберите одну
    set_property(TARGET GameA PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # Устанавливаем рабочую директорию для отладчика Visual Studio в папку с exe-файлом
    set_property(TARGET GameA PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GameA>")
endif()
