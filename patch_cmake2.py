import sys
with open('core/libs/CMakeLists.txt', 'r') as f:
    content = f.read()

# I added this block for libc earlier:
# if(APPLE)
#     set(LIBC_ALIAS_FILE ${CMAKE_CURRENT_BINARY_DIR}/libc_aliases.txt)
#     ...
# else()
#     ...
# endif()

# Let's find it and remove it, putting back the simple APPLE condition for libc.
# We want:
# if(APPLE)
#     add_custom_command(
#             OUTPUT ${LIBC_PATCHED}
#             COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_UNPATCHED} ${LIBC_PATCHED}
#             DEPENDS libc ${LIBC_UNPATCHED}
#             COMMENT "Copying NID patched for libc on Apple"
#     )
# else()
#     ...

import re
content = re.sub(r"if\(APPLE\)\s*set\(LIBC_ALIAS_FILE.*?-Wl,-alias_list,\$\{LIBC_ALIAS_FILE\}\)\s*add_custom_command", r"if(APPLE)\n    add_custom_command", content, flags=re.DOTALL)

with open('core/libs/CMakeLists.txt', 'w') as f:
    f.write(content)
