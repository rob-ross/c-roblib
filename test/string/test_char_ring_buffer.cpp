//  test_string_utils.cpp
// Created by Rob Ross on 7/23/26.
//

#include "gtest/gtest.h"
#include "roblib/base.h"

#include "roblib/char_ring_buffer.h"

extern "C" {
}

CHAR_RING_BUFFER(CharRingBuffer, 10);

TEST(CharRingBuffer10, TestAdd) {
    EXPECT_EQ(sizeof(CharRingBuffer10::buffer), 10);
    CharRingBuffer10 crb{}; // Direct-list-initialization
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 10, "abcdefghij");
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    crb = {};

    // It's good practice to verify the reset worked as expected
    EXPECT_EQ(crb.buffer[0], '\0');
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 5, "abcde");
    EXPECT_EQ(crb.length, 5);
    EXPECT_EQ(crb.end_index, 5);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 10, "fghijklmno");
    EXPECT_EQ(crb.length, 10);
    EXPECT_EQ(crb.end_index, 5);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);
}

TEST(CharRingBuffer10, TestRead) {
    CharRingBuffer10 crb{}; // Direct-list-initialization

    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 10, "abcdefghij");
    EXPECT_EQ(crb.length, 10);
    EXPECT_EQ(crb.start_index, 0);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    // read 4 values
    for (int i = 0; i < 4; ++i) {
        pvt_crb_get_next_char_CharRingBuffer10(&crb);
    }
    EXPECT_EQ(crb.length, 6);
    EXPECT_EQ(crb.start_index, 4);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    // read 6 values
    for (int i = 0; i < 6; ++i) {
        pvt_crb_get_next_char_CharRingBuffer10(&crb);
    }
    EXPECT_EQ(pvt_crb_get_next_char_CharRingBuffer10(&crb), EOF);
    EXPECT_EQ(crb.length, 0);
    EXPECT_EQ(crb.start_index, 0);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);
}

TEST(CharRingBuffer10, TestAddReadAddRead) {
    CharRingBuffer10 crb{}; // Direct-list-initialization
    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 10, "abcdefghij");
    EXPECT_EQ(crb.length, 10);
    EXPECT_EQ(crb.start_index, 0);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    // read 6 values
    for (int i = 0; i < 6; ++i) {
        pvt_crb_get_next_char_CharRingBuffer10(&crb);
    }
    EXPECT_EQ(crb.length, 4);
    EXPECT_EQ(crb.start_index, 6);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);

    // write 12 values - should only add the last 6
    pvt_crb_add_str_to_buffer_CharRingBuffer10(&crb, 12, "123456789XYZ");
    EXPECT_EQ(crb.length, 10);
    EXPECT_EQ(crb.start_index, 0);
    EXPECT_EQ(crb.end_index, 0);
    //pvt_crb_print_repr_CharRingBuffer10(&crb);
}
