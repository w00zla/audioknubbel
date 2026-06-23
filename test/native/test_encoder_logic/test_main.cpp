#include <unity.h>
#include "../../../src/encoder.h"

void setUp() {}
void tearDown() {}

void test_cw_full_cycle() {
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b00, 0b01));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b01, 0b11));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b11, 0b10));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b10, 0b00));
}

void test_ccw_full_cycle() {
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b00, 0b10));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b10, 0b11));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b11, 0b01));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b01, 0b00));
}

void test_no_change() {
    TEST_ASSERT_EQUAL(0, encoderQuadStep(0b00, 0b00));
    TEST_ASSERT_EQUAL(0, encoderQuadStep(0b11, 0b11));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cw_full_cycle);
    RUN_TEST(test_ccw_full_cycle);
    RUN_TEST(test_no_change);
    return UNITY_END();
}
