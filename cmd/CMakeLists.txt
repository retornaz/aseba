add_executable(asebacmd
	cmd.cpp
	../${TRANSPORT_PLUGIN}
)

target_link_libraries(asebacmd asebacore ${EXTRA_LIBS})

install(TARGETS asebacmd RUNTIME
	DESTINATION bin
	LIBRARY DESTINATION bin
)
