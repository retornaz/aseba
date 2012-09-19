add_executable(asebadump
	dump.cpp
	../${TRANSPORT_PLUGIN}
)

target_link_libraries(asebadump asebacore ${EXTRA_LIBS})

install(TARGETS asebadump RUNTIME
	DESTINATION bin
	LIBRARY DESTINATION lib
)
