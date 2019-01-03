//
// Created by john on 1/2/19.
//

#include <stdio.h>
#include <check.h>

#include "zectl_tests.h"
#include "libze/libze.h"

START_TEST (test_libze_init)
    {
        libze_handle_t *lzeh = NULL;
        lzeh = libze_init();

        fail_if(!lzeh, "lzeh null after allocation");
    }
END_TEST

Suite* zectl_suite(void) {
    Suite *suite = suite_create("zectl");
    TCase *tcase = tcase_create("case");
    tcase_add_test(tcase, test_libze_init);
    suite_add_tcase(suite, tcase);
    return suite;
}

int main (int argc, char *argv[]) {
    int number_failed;
    Suite *suite = zectl_suite();
    SRunner *runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return number_failed;
}