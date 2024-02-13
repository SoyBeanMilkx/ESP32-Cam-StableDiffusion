file(GLOB Utils "utils/*.c" "utils/*.h")

idf_component_register(SRCS "main.c" ${Utils}
        INCLUDE_DIRS "")
