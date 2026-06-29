#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

typedef struct
{
    int passed;
    int total;
} test_result_t;

/* Per-module test runners. Each runs all of its module's tests and returns the
 * passed/total tally. */
test_result_t test_tm_run_all(void);
test_result_t test_tc_run_all(void);

#endif /* TEST_RUNNERS_H */
