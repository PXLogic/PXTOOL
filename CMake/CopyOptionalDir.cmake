if(EXISTS "${SRC}")
    file(MAKE_DIRECTORY "${DST}")
    file(COPY "${SRC}/" DESTINATION "${DST}")
endif()
