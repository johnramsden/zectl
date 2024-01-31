# zectl Manpages ------------------------------------------------------------------
option(BUILD_DOCS "Create the manpage files" YES)
set(LIST_OF_MANPAGE_SOURCES
	zectl.8.scd
)

if (BUILD_DOCS)
	find_program(SCDOC scdoc)

	foreach(PAGE_SOURCE_NAME ${LIST_OF_MANPAGE_SOURCES})
		# Rebuild the output filename (remove .scd extension)
		string(REPLACE ".scd" "" MANPAGE_FILE_NAME ${PAGE_SOURCE_NAME})

		add_custom_command(
			OUTPUT ${MANPAGE_FILE_NAME}
			COMMAND ${SCDOC} < ${CMAKE_CURRENT_SOURCE_DIR}/${PAGE_SOURCE_NAME} > ${CMAKE_BINARY_DIR}/docs/${MANPAGE_FILE_NAME}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${PAGE_SOURCE_NAME}
			COMMENT "Generating man page : ${PAGE_SOURCE_NAME} -> ${MANPAGE_FILE_NAME}"
		)
		list(APPEND GENERATED_MAN_PAGES ${MANPAGE_FILE_NAME})
	endforeach()

	add_custom_target(
		manpages
		ALL
		DEPENDS ${GENERATED_MAN_PAGES}
		SOURCES ${LIST_OF_MANPAGE_SOURCES}
	)
endif()

