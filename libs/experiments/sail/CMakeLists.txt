set(NAME exp_sail)

set(SOURCES
    sail.cpp
)

add_library(${NAME} STATIC ${SOURCES})

target_link_libraries(${NAME} 
	base
	experiments
	fs
	adcs
	gyro
	payload
	power
	photo
	time
	exp_fs
	comm
	telecommunication
)

target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Include/experiment/sail)
