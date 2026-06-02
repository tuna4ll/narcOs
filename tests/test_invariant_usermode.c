#include <check.h>
#include <stdlib.h>
#include <string.h>

/* Declaration of the function under test */
extern int run_user_fetch(const char* args);

/* Mock to check if we're in kernel context - in real kernel this would be provided */
extern int is_kernel_context(void);

START_TEST(test_user_fetch_rejects_kernel_context_execution)
{
    /* Invariant: run_user_fetch must NEVER execute HTTP fetches in kernel context.
     * Any URL provided by userspace must be rejected or delegated to userspace handler. */
    const char *payloads[] = {
        "http://malicious.attacker.com/exploit",  /* Attacker-controlled server */
        "http://127.0.0.1:8080/local",            /* Localhost boundary case */
        "http://example.com/valid",               /* Valid benign input */
        "",                                        /* Empty input boundary */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        int result = run_user_fetch(payloads[i]);
        
        /* Security invariant: function must either:
         * 1. Return error code (negative) rejecting the operation, OR
         * 2. Not be executing in kernel context when processing network data */
        ck_assert_msg(result < 0 || !is_kernel_context(),
            "SECURITY VIOLATION: run_user_fetch processed '%s' in kernel context",
            payloads[i]);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_user_fetch_rejects_kernel_context_execution);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}