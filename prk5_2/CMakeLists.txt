# Настройки для версии с именованными каналами
add_executable(prk5_2 prk5_2.c)

# Дополнительные флаги компиляции
target_compile_options(prk5_2 PRIVATE -Wpedantic)

# Для Astra Linux может потребоваться
target_compile_definitions(prk5_2 PRIVATE _GNU_SOURCE)
