# Copyright (c) 2024 Michał Łyszczek
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_PSMQ)

	set(PSMQ_DIR ${ZEPHYR_CURRENT_MODULE_DIR})

	zephyr_library()

	zephyr_include_directories(${PSMQ_DIR}/lib)

	zephyr_library_sources(
		${PSMQ_DIR}/src/broker.c
	)

endif()
