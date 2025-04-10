cmake_minimum_required(VERSION 3.10)
project(prk5)

# Общие настройки для всего проекта
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Включаем подпроекты
add_subdirectory(prk5_1)
add_subdirectory(prk5_2)

# Дополнительная цель для сборки всего
add_custom_target(all_combined DEPENDS prk5_1 prk5_2)
