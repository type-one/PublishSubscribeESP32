set(API_TESTS
  test_abandon.cpp
  test_algo_adapters.cpp
  test_async.cpp
  test_cancelation.cpp
  test_future.cpp
  test_future_next.cpp
  test_future_then.cpp
  test_future_then_unwrap.cpp
  test_notify.cpp
  test_packaged_task.cpp
  test_packaged_task_unwrap.cpp
  test_promise.cpp
  test_small_unique_function.cpp
  test_shared_future.cpp
  test_shared_future_next.cpp
  test_shared_future_then.cpp
  test_shared_future_then_unwrap.cpp
  test_thread_pool.cpp
  test_timed_waiter.cpp
  test_unique_function.cpp
  test_when_all_tuple.cpp
  test_when_all_vector.cpp
  test_when_any_tuple.cpp
  test_when_any_vector.cpp
)

set(DETAILS_TESTS
  test_either.cpp
  test_once_consumable_stack.cpp
)

set(TEST_TOOLS
  test_helpers.h
  test_tools.h
  test_tools.cpp
  simple_arena_allocator.h
)

add_executable(pc_unit_tests ${API_TESTS} ${DETAILS_TESTS} ${TEST_TOOLS})
find_package(GTest REQUIRED)
target_link_libraries(pc_unit_tests portable_concurrency GTest::gtest GTest::gtest_main)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7)
    # Useless warning on correct code which marks the problem which can only happen
    # when linking code compiled with -std=c++14 and -std=c++17 flags together.
    target_compile_options(pc_unit_tests PRIVATE -Wno-noexcept-type)
  endif()
  if (NOT PC_DEV_BUILD)
    target_compile_options(pc_unit_tests PRIVATE -Wno-unused-result)
  endif()
endif()
if (MSVC)
  target_compile_definitions(pc_unit_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  if (PC_NO_DEPRECATED)
    target_compile_options(pc_unit_tests PRIVATE -Werror=deprecated-declarations)
  else()
    target_compile_options(pc_unit_tests PRIVATE -Wno-deprecated-declarations)
  endif()
endif()
add_test(NAME pc_unit_tests
  COMMAND pc_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/Reports/pc_unit_tests.xml
)
