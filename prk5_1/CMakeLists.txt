# Настройки для версии с сигналами
add_executable(prk5_1 prk5_1.c)

# Для работы с сигналами реального времени
target_link_libraries(prk5_1 rt)

# Установка POSIX стандарта
target_compile_definitions(prk5_1 PRIVATE _POSIX_C_SOURCE=200809L)
