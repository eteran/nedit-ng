function(GENERATE_TRANSLATION_RESOURCE_FILE FILENAME QM_FILES)
    message(STATUS "Generating translation resource file ${FILENAME}")

    file(WRITE ${FILENAME} "<RCC>\n\t<qresource prefix=\"/i18n\">")
	foreach(QM_FILE ${QM_FILES})
		get_filename_component(QM_FILE_NAME ${QM_FILE} NAME)
        message(STATUS "Adding translation file ${QM_FILE_NAME}")
		file(APPEND ${FILENAME} "\n\t\t<file>${QM_FILE_NAME}</file>")
	endforeach()

	file(APPEND ${FILENAME} "\n\t</qresource>\n</RCC>")
endfunction()
