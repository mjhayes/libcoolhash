#include <stdlib.h>
#include <check.h>

#include "../src/coolhash.h"

START_TEST(test_coolhash_new)
{
        struct coolhash *ch;
        struct coolhash_profile profile;

        profile.size = 16;
        profile.shards = 4;

        ch = coolhash_new(&profile);
        coolhash_free(ch);
}
END_TEST

Suite *coolhash_suite(void)
{
        Suite *s;
        TCase *tc_core;

        s = suite_create("coolhash");
        tc_core = tcase_create("Core");

        tcase_add_test(tc_core, test_coolhash_new);
        suite_add_tcase(s, tc_core);

        return s;
}

int main(void)
{
        int number_failed;
        Suite *s;
        SRunner *sr;

        s = coolhash_suite();
        sr = srunner_create(s);

        srunner_run_all(sr, CK_NORMAL);
        number_failed = srunner_ntests_failed(sr);
        srunner_free(sr);
        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set et ts=8 sw=8 sts=8: */
