# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compiler warnings for Zarescope's own targets. Third-party code never gets
# these flags, and its headers are included as SYSTEM so they don't trip them
# either (cmake/Dependencies.cmake).

option(ZARESCOPE_WERROR "Treat warnings in Zarescope's own code as errors" ON)

function(zarescope_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(ZARESCOPE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(ZARESCOPE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
