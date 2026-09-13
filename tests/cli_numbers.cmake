# Check both the exit status and diagnostic so a crash cannot pass as rejection.
foreach(option IN ITEMS
    "-trials=1garbage" "-trials=2147483648" "-trials="
    "-restarts=1.5" "-verbose=0junk" "-budget=9junk"
    "-num_iterations=1e9" "-num_iterations=9223372036854775808"
    "-seed=-1" "-seed= -1" "-seed=18446744073709551616" "-seed=1junk"
    "-max_time=0junk" "-epsilon=0.5junk" "-reopt=0.5junk")
  execute_process(COMMAND "${GLNS_CLI}" "${GLNS_INSTANCE}" "${option}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result STREQUAL "0" OR NOT error MATCHES "error: invalid value for")
    message(FATAL_ERROR "${option}: expected validation error, got ${result}: ${error}")
  endif()
endforeach()

# Exercise the progress bar with maximal counters, and valid numeric boundaries.
execute_process(COMMAND "${GLNS_CLI}" "${GLNS_INSTANCE}"
    -trials=2147483647 -restarts=2147483647 -max_time=0
    -seed=18446744073709551615 -budget=9223372036854775807
    -epsilon=5e-1 -reopt=1.0 -verbose=3
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result STREQUAL "0" OR NOT error STREQUAL "")
  message(FATAL_ERROR "Valid boundary options failed: ${result}: ${error}")
endif()
if(NOT output MATCHES "RNG Seed *: 18446744073709551615" OR
   NOT output MATCHES "Tour is Feasible\\? *: true")
  message(FATAL_ERROR "Unexpected output for valid boundary options: ${output}")
endif()
