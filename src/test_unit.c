//
// Created by Rob Ross on 2/6/26.
//


#include "unit.h"

void t1() {
    assertTrue(1==1);
    assertTrue(1==1, "Very true!");
    assertTrue(1==0, "Failure message.");
}

void t2() {
    assertTrue(1==1);
    assertTrue(1==1, "Very true!");
}

void t3() {
    // assertEquals tests
    // assertEqual('c', 'c');
}

void test_equal_char_literal() {
    assertEqual('a', 'a');
    // in C, char literals are int!!!!
    assertEqual((char)'a', (char)'b', "should fail");
}

void test_equal_char() {
    char c1 = 'a';
    char c2 = 'a';
    char c3 = 'b';
    assertEqual(c1, c2);
    assertEqual(c1, c3, "should fail");
}

void test_equal_signed_char() {
    signed char uc1 = 'a';
    signed char uc2 = 'a';
    signed char uc3 = 'b';
    assertEqual(uc1, uc2);
    assertEqual(uc1, uc3, "should fail");
}

void test_equal_uchar() {
    unsigned char uc1 = 'a';
    unsigned char uc2 = 'a';
    unsigned char uc3 = 'b';
    assertEqual(uc1, uc2);
    assertEqual(uc1, uc3, "should fail");
}

void test_short_literal() {
    assertEqual((short) 22, (short)22);
    assertEqual((short) 22u, (short)22u);
}
void test_equal_short() {

    short s1 = 42;
    short s2 = 42;
    short s3 = 67;
    assertEqual(s1, s2);
    assertEqual(s1, s3, "should fail");
}

void test_equal_ushort() {
    assertEqual((unsigned short) 22, (unsigned short)22);
    unsigned short s1 = 42u;
    unsigned short s2 = 42u;
    unsigned short s3 = 67u;
    assertEqual(s1, s2);
    assertEqual(s1, s3, "should fail");
}

void test_equal_int_literal() {
    assertEqual( 42, 42);
    assertEqual( 42u, 42u);
}

void test_equal_int() {
    int i1 = 42;
    int i2 = 42;
    int i3 = 67;
    assertEqual(i1, i2);
    assertEqual(i1, i3, "should fail");
}

void test_equal_uint() {
    unsigned int i1 = 42;
    unsigned int i2 = 42;
    unsigned int i3 = 67;
    assertEqual(i1, i2);
    assertEqual(i1, i3, "should fail");
}

void test_equal_long_literal() {
    assertEqual( 42L, 42L);
    assertEqual( 42Lu, 42Lu);
}

void test_equal_long() {
    long l1 = 42;
    long l2 = 42;
    long l3 = 43;
    assertEqual(l1, l2);
    assertEqual(l1, l3, "should fail");
}

void test_equal_ulong() {
    unsigned long l1 = 42;
    unsigned long l2 = 42;
    unsigned long l3 = 43;
    assertEqual(l1, l2);
    assertEqual(l1, l3, "should fail");
}

void test_equal_long__long_literal() {
    assertEqual( 42LL, 42LL);
    assertEqual( 42LLu, 42LLu);
}

void test_equal_long_long() {
    long long l1 = 42;
    long long l2 = 42;
    long long l3 = 43;
    assertEqual(l1, l2);
    assertEqual(l1, l3, "should fail");
}

void test_equal_ulong_long() {
    unsigned long long l1 = 42;
    unsigned long long l2 = 42;
    unsigned long long l3 = 43;
    assertEqual(l1, l2);

    assertThrows( assertEqual(l1, l3, "should fail") );
}


// make:   clang -std=c17 -o test_unit.out test_unit.c unit.c


int main(void) {
    // test_case tests[] = {t1, t2, t3, NULL};
    //test_equal_char_literal();
    test_case equal_tests[] = {
        test_equal_char_literal, test_equal_char, test_equal_signed_char, test_equal_uchar,
        test_short_literal, test_equal_short, test_equal_ushort,
        test_equal_int_literal, test_equal_int, test_equal_uint,
        test_equal_long_literal, test_equal_long, test_equal_ulong,
        test_equal_long__long_literal, test_equal_long_long, test_equal_ulong_long, NULL};

    test_runner(equal_tests);


    return 0;
}
