#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include "../src/coolhash.h"

START_TEST(test_coolhash_new)
{
        struct coolhash *ch;
        struct coolhash_profile profile;

        coolhash_profile_set_size(&profile, 16);
        coolhash_profile_set_shards(&profile, 4);
        coolhash_profile_set_load_factor(&profile, 80);

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_uint_eq(coolhash_profile_get_size(&ch->profile), 16);
        ck_assert_uint_eq(coolhash_profile_get_shards(&ch->profile), 4);
        ck_assert_int_eq(coolhash_profile_get_load_factor(&ch->profile), 80);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_new_invalid_size_shards)
{
        struct coolhash *ch;
        struct coolhash_profile profile;

        /* size and shards with invalid values */
        coolhash_profile_set_size(&profile, 0);
        coolhash_profile_set_shards(&profile, 0);
        coolhash_profile_set_load_factor(&profile, 0);

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_uint_eq(ch->profile.size, 1);
        ck_assert_uint_eq(ch->profile.shards, 1);
        ck_assert_int_eq(ch->profile.load_factor, 80);
        coolhash_free(ch);

        /* size less than shards */
        coolhash_profile_set_size(&profile, 1);
        coolhash_profile_set_shards(&profile, 4);

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_uint_eq(ch->profile.size, 4); /* should be equal to
                                                      shards */
        ck_assert_uint_eq(ch->profile.shards, 4);
        coolhash_free(ch);

        /* size not divisible by shards */
        coolhash_profile_set_size(&profile, 10);
        coolhash_profile_set_shards(&profile, 4);

        ch = coolhash_new(&profile);
        ck_assert_ptr_ne(ch, NULL);
        ck_assert_uint_eq(ch->profile.size, 12); /* rounded up */
        ck_assert_uint_eq(ch->profile.shards, 4);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_set_get)
{
        struct coolhash *ch;
        int res;
        int *data;
        void *lock;

        ch = coolhash_new(NULL);
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

        coolhash_unlock(ch, lock);
        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_set_del)
{
        struct coolhash *ch;
        int res, var;
        int *data;
        void *lock;

        ch = coolhash_new(NULL);
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

static void test_coolhash_foreach_cb(struct coolhash *ch, coolhash_key_t key,
                void *data, void *lock, void *cb_arg)
{
        *((int *) cb_arg) += *((int *) data);
        coolhash_unlock(ch, lock);
}

START_TEST(test_coolhash_foreach)
{
        struct coolhash *ch;
        int res, var1, var2, var3, var4, cb_arg;

        ch = coolhash_new(NULL);
        ck_assert_ptr_ne(ch, NULL);

        var1 = 7;
        res = coolhash_set(ch, 0, &var1);
        ck_assert_int_eq(res, 0);

        var2 = 3;
        res = coolhash_set(ch, 1, &var2);
        ck_assert_int_eq(res, 0);

        var3 = 4;
        res = coolhash_set(ch, 2, &var3);
        ck_assert_int_eq(res, 0);

        var4 = 5;
        res = coolhash_set(ch, 3, &var4);
        ck_assert_int_eq(res, 0);

        cb_arg = 12;
        /* Our callback increments cb_arg by each variable defined above */
        coolhash_foreach(ch, test_coolhash_foreach_cb, &cb_arg);
        ck_assert_int_eq(cb_arg, 31); /* This should prove cb_arg works too */

        /* Let's call it again to make sure everything got properly
         * unlocked! */
        coolhash_foreach(ch, test_coolhash_foreach_cb, &cb_arg);
        ck_assert_int_eq(cb_arg, 50);

        coolhash_free(ch);
}
END_TEST

START_TEST(test_coolhash_auto_rehash)
{
        struct coolhash *ch;
        struct coolhash_profile profile;
        int res, var1, var2, var3, var4, cpy;

        coolhash_profile_set_size(&profile, 16);
        coolhash_profile_set_shards(&profile, 4);
        coolhash_profile_set_load_factor(&profile, 80);

        ch = coolhash_new(&profile);

        /* Since we have 4 shards, each table will initially have a size
         * of 4 (16 / 4). Since our load factor is 80%, we will actually need
         * to insert 4 items into the first shard (4 * (int) .8 + 1) == 4. */
        var1 = 1;
        res = coolhash_set(ch, 0, &var1);
        ck_assert_int_eq(res, 0);

        var2 = 2;
        res = coolhash_set(ch, 4, &var2);
        ck_assert_int_eq(res, 0);

        var3 = 3;
        res = coolhash_set(ch, 8, &var3);
        ck_assert_int_eq(res, 0);

        var4 = 4;
        res = coolhash_set(ch, 12, &var4);
        ck_assert_int_eq(res, 0);

        /* All the items should be in table shard [0] (0 % 4 == 0, etc.) */
        ck_assert_uint_eq(ch->tables[0].size, 8); /* Should have doubled */

        /* Make sure we can retrieve our items */
        res = coolhash_get_copy(ch, 0, &cpy, sizeof(cpy));
        ck_assert_int_eq(res, 0);
        ck_assert_int_eq(cpy, 1);

        res = coolhash_get_copy(ch, 4, &cpy, sizeof(cpy));
        ck_assert_int_eq(res, 0);
        ck_assert_int_eq(cpy, 2);

        res = coolhash_get_copy(ch, 8, &cpy, sizeof(cpy));
        ck_assert_int_eq(res, 0);
        ck_assert_int_eq(cpy, 3);

        res = coolhash_get_copy(ch, 12, &cpy, sizeof(cpy));
        ck_assert_int_eq(res, 0);
        ck_assert_int_eq(cpy, 4);

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
        tcase_add_test(tc_core, test_coolhash_foreach);
        tcase_add_test(tc_core, test_coolhash_auto_rehash);
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
