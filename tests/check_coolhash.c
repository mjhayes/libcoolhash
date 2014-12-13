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
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_int_eq(ch->profile.size, 16);
        ck_assert_int_eq(ch->profile.shards, 4);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_new_invalid_size_shards)
{
        struct coolhash *ch;
        struct coolhash_profile profile;

        /* size and shards with invalid values */
        profile.size = -4;
        profile.shards = 0;

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_int_eq(ch->profile.size, 1);
        ck_assert_int_eq(ch->profile.shards, 1);
        coolhash_free(ch);

        /* size less than shards */
        profile.size = 1;
        profile.shards = 4;

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_int_eq(ch->profile.size, 4); /* should be equal to
                                                      shards */
        ck_assert_int_eq(ch->profile.shards, 4);
        coolhash_free(ch);

        /* size not divisible by shards */
        profile.size = 10;
        profile.shards = 4;

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_int_eq(ch->profile.size, 12); /* rounded up */
        ck_assert_int_eq(ch->profile.shards, 4);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_set_get)
{
        struct coolhash *ch;
        struct coolhash_profile profile;
        int res;
        int *data;
        void *lock;

        profile.size = 16;
        profile.shards = 4;

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);

        res = coolhash_set(NULL, 0, &res); /* NULL instance, should fail */
        ck_assert_int_ne(res, 0);

        res = coolhash_set(ch, 0, NULL); /* NULL data, should fail */
        ck_assert_int_ne(res, 0);

        res = coolhash_set(ch, 0, &res); /* Should work */
        ck_assert_int_eq(res, 0);

        data = coolhash_get(NULL, 0, &lock); /* NULL instance, should fail */
        ck_assert_ptr_eq(data, NULL);

        data = coolhash_get(ch, 0, NULL); /* NULL lock, should fail */
        ck_assert_ptr_eq(data, NULL);

        data = coolhash_get(ch, 0, &lock); /* Should work */
        ck_assert_ptr_ne(data, NULL);

        /* *data should equal 0 */
        ck_assert_int_eq(*data, 0);

        coolhash_unlock(lock);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_set_del)
{
        struct coolhash *ch;
        struct coolhash_profile profile;
        int res, var;
        int *data;
        void *lock;

        profile.size = 16;
        profile.shards = 4;

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);

        var = 7;
        res = coolhash_set(ch, 5, &var);
        ck_assert_int_eq(res, 0);

        data = coolhash_get(ch, 5, &lock);
        ck_assert_ptr_ne(data, NULL);
        ck_assert_int_eq(*data, 7);

        coolhash_del(ch, lock);

        data = coolhash_get(ch, 5, &lock); /* Should not be accessible */
        ck_assert_ptr_eq(data, NULL);

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
        tcase_add_test(tc_core, test_coolhash_new_invalid_size_shards);
        tcase_add_test(tc_core, test_coolhash_set_get);
        tcase_add_test(tc_core, test_coolhash_set_del);
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
