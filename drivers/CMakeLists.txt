cmake_minimum_required(VERSION 3.20.0)

add_subdirectory(display)

# The upstream display library has no sources when using
# an out-of-tree display driver.
if(TARGET drivers__display)
    set_property(
        TARGET drivers__display
        PROPERTY ALLOW_EMPTY TRUE
    )
endif()
