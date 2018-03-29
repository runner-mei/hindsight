# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

configure_file(test.h.in test.h ESCAPE_QUOTES)
include_directories(${CMAKE_CURRENT_BINARY_DIR})


set(HINDSIGHT_TEST_CONFIG_SRC
../hs_config.c
../hs_logger.c
../hs_checkpoint_reader.c
../hs_util.c
test_config.c
)

if(MSVC)
set(HINDSIGHT_TEST_CONFIG_SRC ${HINDSIGHT_TEST_CONFIG_SRC}
../win_commons.h
../win_commons.c
../win_pthread.h
../win_pthread.c
../win_sem.h
../win_sem.c
../win_dirent.h
../win_dirent.c
)
endif()

add_executable(test_config ${HINDSIGHT_TEST_CONFIG_SRC})
target_include_directories(test_config  PRIVATE ${LUASANDBOX_INCLUDE_DIR})
target_link_libraries(test_config ${HINDSIGHT_LIBS})
add_test(NAME test_config WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND test_config)

