# Copyright (c) 2024 Michał Łyszczek
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_PSMQ)

set(PSMQ_DIR ${ZEPHYR_CURRENT_MODULE_DIR})

zephyr_library()

zephyr_include_directories(
	${PSMQ_DIR}
	${PSMQ_DIR}/inc
	${PSMQ_DIR}/src

	${ZEPHYR_BASE}/include
	${ZEPHYR_BASE}/include/zephyr/posix
)

set(PSMQ_MSG_MAX ${CONFIG_PSMQ_MSG_MAX})
configure_file(${PSMQ_DIR}/inc/psmq.h.in ${PSMQ_DIR}/inc/psmq.h @ONLY)

# mandatory files
zephyr_library_sources(
	${PSMQ_DIR}/lib/psmq.c
	${PSMQ_DIR}/src/broker.c
	${PSMQ_DIR}/src/cfg.c
	${PSMQ_DIR}/src/globals.c
	${PSMQ_DIR}/src/topic-list.c
	${PSMQ_DIR}/src/utils.c
	${PSMQ_DIR}/src/psmqd.c
)

zephyr_library_sources_ifdef(CONFIG_PSMQ_TOOLS_SUB
	${PSMQ_DIR}/src/psmq-sub.c
	${PSMQ_DIR}/zephyr/psmq_shell.c
)

zephyr_library_sources_ifdef(CONFIG_PSMQ_TOOLS_PUB
	${PSMQ_DIR}/src/psmq-pub.c
	${PSMQ_DIR}/zephyr/psmq_shell.c
)

zephyr_library_compile_definitions(PSMQ_MAX_CLIENTS=${CONFIG_PSMQ_MAX_CLIENTS})
zephyr_library_compile_definitions(PSMQ_LIBRARY=0)
zephyr_library_compile_definitions(PSMQ_STANDALONE=0)
zephyr_library_compile_definitions(PSMQ_NO_OPTERR=0)
zephyr_library_compile_definitions(PSMQ_NO_SIGNALS=1)
zephyr_library_compile_definitions(STDIN_FILENO=0)
zephyr_library_compile_definitions(PACKAGE_STRING="psmq v0.2.1")
zephyr_library_compile_definitions(PACKAGE_VERSION="v0.2.1")


endif()
