import sys
with open('core/libs/CMakeLists.txt', 'r') as f:
    content = f.read()

macro_code = """
if(APPLE)
    macro(add_library target_name)
        _add_library(${target_name} ${ARGN})
        # Only apply to PRX libraries (we check if it's not ALIAS or INTERFACE or IMPORTED)
        get_target_property(type ${target_name} TYPE)
        if(type STREQUAL "SHARED_LIBRARY")
            set(ALIAS_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_aliases.txt)
            add_custom_command(TARGET ${target_name} PRE_LINK
                COMMAND python3 ${PROJECT_SOURCE_DIR}/macos_nid_alias.py ${ALIAS_FILE} ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating NID aliases for ${target_name}"
            )
            target_link_options(${target_name} PRIVATE -Wl,-alias_list,${ALIAS_FILE})
        endif()
    endmacro()
endif()
"""

# Remove existing macro
start = content.find("if(APPLE)\n    macro(add_library target_name)")
end = content.find("endif()\n\nset(UNPATCHED_DIR", start)
if start != -1 and end != -1:
    content = content[:start] + content[end + 8:]

# Insert before libc
libc_pos = content.find("add_library(libc SHARED libc/Export.cpp)")
content = content[:libc_pos] + macro_code + content[libc_pos:]

with open('core/libs/CMakeLists.txt', 'w') as f:
    f.write(content)
