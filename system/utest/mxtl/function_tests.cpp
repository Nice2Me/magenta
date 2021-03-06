// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mxtl/function.h>
#include <mxtl/vector.h>
#include <unittest/unittest.h>

namespace mxtl {
namespace tests {
namespace {

using Closure = void();
using BinaryOp = int(int a, int b);
using MoveOp = mxtl::unique_ptr<int>(mxtl::unique_ptr<int> value);

// A big object which causes a function target to be heap allocated.
struct Big {
    int data[64]{};
};

constexpr size_t HugeCallableSize = sizeof(Big) + sizeof(void*) * 4;

template <typename ClosureFunction>
bool closure() {
    // default initialization
    ClosureFunction fdefault;
    EXPECT_FALSE(!!fdefault);

    // nullptr initialization
    ClosureFunction fnull(nullptr);
    EXPECT_FALSE(!!fnull);

    // inline callable initialization
    int finline_value = 0;
    ClosureFunction finline([&finline_value] { finline_value++; });
    EXPECT_TRUE(!!finline);
    finline();
    EXPECT_EQ(1, finline_value);
    finline();
    EXPECT_EQ(2, finline_value);

    // heap callable initialization
    int fheap_value = 0;
    ClosureFunction fheap([&fheap_value, big = Big() ] { fheap_value++; });
    EXPECT_TRUE(!!fheap);
    fheap();
    EXPECT_EQ(1, fheap_value);
    fheap();
    EXPECT_EQ(2, fheap_value);

    // move initialization of a nullptr
    ClosureFunction fnull2(mxtl::move(fnull));
    EXPECT_FALSE(!!fnull2);

    // move initialization of an inline callable
    ClosureFunction finline2(mxtl::move(finline));
    EXPECT_TRUE(!!finline2);
    EXPECT_FALSE(!!finline);
    finline2();
    EXPECT_EQ(3, finline_value);
    finline2();
    EXPECT_EQ(4, finline_value);

    // move initialization of a heap callable
    ClosureFunction fheap2(mxtl::move(fheap));
    EXPECT_TRUE(!!fheap2);
    EXPECT_FALSE(!!fheap);
    fheap2();
    EXPECT_EQ(3, fheap_value);
    fheap2();
    EXPECT_EQ(4, fheap_value);

    // inline mutable lambda
    int fmutinline_value = 0;
    ClosureFunction fmutinline([&fmutinline_value, x = 1 ]() mutable {
        x *= 2;
        fmutinline_value = x;
    });
    EXPECT_TRUE(!!fmutinline);
    fmutinline();
    EXPECT_EQ(2, fmutinline_value);
    fmutinline();
    EXPECT_EQ(4, fmutinline_value);

    // heap-allocated mutable lambda
    int fmutheap_value = 0;
    ClosureFunction fmutheap([&fmutheap_value, big = Big(), x = 1 ]() mutable {
        x *= 2;
        fmutheap_value = x;
    });
    EXPECT_TRUE(!!fmutheap);
    fmutheap();
    EXPECT_EQ(2, fmutheap_value);
    fmutheap();
    EXPECT_EQ(4, fmutheap_value);

    // move assignment of non-null
    ClosureFunction fnew([] {});
    fnew = mxtl::move(finline2);
    EXPECT_TRUE(!!fnew);
    fnew();
    EXPECT_EQ(5, finline_value);
    fnew();
    EXPECT_EQ(6, finline_value);

    // move assignment of null
    fnew = mxtl::move(fnull);
    EXPECT_FALSE(!!fnew);

    // callable assignment with operator=
    int fnew_value = 0;
    fnew = [&fnew_value] { fnew_value++; };
    EXPECT_TRUE(!!fnew);
    fnew();
    EXPECT_EQ(1, fnew_value);
    fnew();
    EXPECT_EQ(2, fnew_value);

    // callable assignment with SetTarget
    fnew.SetTarget([&fnew_value] { fnew_value *= 2; });
    EXPECT_TRUE(!!fnew);
    fnew();
    EXPECT_EQ(4, fnew_value);
    fnew();
    EXPECT_EQ(8, fnew_value);

    // nullptr assignment
    fnew = nullptr;
    EXPECT_FALSE(!!fnew);

    // swap (currently null)
    fnew.swap(fheap2);
    EXPECT_TRUE(!!fnew);
    EXPECT_FALSE(!!fheap);
    fnew();
    EXPECT_EQ(5, fheap_value);
    fnew();
    EXPECT_EQ(6, fheap_value);

    // swap with self
    fnew.swap(fnew);
    EXPECT_TRUE(!!fnew);
    fnew();
    EXPECT_EQ(7, fheap_value);
    fnew();
    EXPECT_EQ(8, fheap_value);

    // swap with non-null
    fnew.swap(fmutinline);
    EXPECT_TRUE(!!fmutinline);
    EXPECT_TRUE(!!fnew);
    fmutinline();
    EXPECT_EQ(9, fheap_value);
    fmutinline();
    EXPECT_EQ(10, fheap_value);
    fnew();
    EXPECT_EQ(8, fmutinline_value);
    fnew();
    EXPECT_EQ(16, fmutinline_value);

    // nullptr comparison operators
    EXPECT_TRUE(fnull == nullptr);
    EXPECT_FALSE(fnull != nullptr);
    EXPECT_TRUE(nullptr == fnull);
    EXPECT_FALSE(nullptr != fnull);
    EXPECT_FALSE(fnew == nullptr);
    EXPECT_TRUE(fnew != nullptr);
    EXPECT_FALSE(nullptr == fnew);
    EXPECT_TRUE(nullptr != fnew);

    // alloc checking constructor, inline
    AllocChecker ac1;
    int fcheck_value = 0;
    ClosureFunction fcheckinline([&fcheck_value] { fcheck_value++; }, &ac1);
    EXPECT_TRUE(!!fcheckinline);
    EXPECT_TRUE(ac1.check());
    fcheckinline();
    EXPECT_EQ(1, fcheck_value);

    // alloc checking set target, inline
    AllocChecker ac2;
    fcheckinline.SetTarget([&fcheck_value] { fcheck_value *= 3; }, &ac2);
    EXPECT_TRUE(!!fcheckinline);
    EXPECT_TRUE(ac2.check());
    fcheckinline();
    EXPECT_EQ(3, fcheck_value);

    // alloc checking constructor, heap allocated
    AllocChecker ac3;
    ClosureFunction fcheckheap([&fcheck_value, big = Big() ] { fcheck_value++; }, &ac3);
    EXPECT_TRUE(!!fcheckheap);
    EXPECT_TRUE(ac3.check());
    fcheckheap();
    EXPECT_EQ(4, fcheck_value);

    // alloc checking set target, heap allocated
    AllocChecker ac4;
    fcheckheap.SetTarget([&fcheck_value, big = Big() ] { fcheck_value *= 3; }, &ac4);
    EXPECT_TRUE(!!fcheckheap);
    EXPECT_TRUE(ac4.check());
    fcheckheap();
    EXPECT_EQ(12, fcheck_value);

    return true;
}

template <typename BinaryOpFunction>
bool binary_op() {
    // default initialization
    BinaryOpFunction fdefault;
    EXPECT_FALSE(!!fdefault);

    // nullptr initialization
    BinaryOpFunction fnull(nullptr);
    EXPECT_FALSE(!!fnull);

    // inline callable initialization
    int finline_value = 0;
    BinaryOpFunction finline([&finline_value](int a, int b) {
        finline_value++;
        return a + b;
    });
    EXPECT_TRUE(!!finline);
    EXPECT_EQ(10, finline(3, 7));
    EXPECT_EQ(1, finline_value);
    EXPECT_EQ(10, finline(3, 7));
    EXPECT_EQ(2, finline_value);

    // heap callable initialization
    int fheap_value = 0;
    BinaryOpFunction fheap([&fheap_value, big = Big() ](int a, int b) {
        fheap_value++;
        return a + b;
    });
    EXPECT_TRUE(!!fheap);
    EXPECT_EQ(10, fheap(3, 7));
    EXPECT_EQ(1, fheap_value);
    EXPECT_EQ(10, fheap(3, 7));
    EXPECT_EQ(2, fheap_value);

    // move initialization of a nullptr
    BinaryOpFunction fnull2(mxtl::move(fnull));
    EXPECT_FALSE(!!fnull2);

    // move initialization of an inline callable
    BinaryOpFunction finline2(mxtl::move(finline));
    EXPECT_TRUE(!!finline2);
    EXPECT_FALSE(!!finline);
    EXPECT_EQ(10, finline2(3, 7));
    EXPECT_EQ(3, finline_value);
    EXPECT_EQ(10, finline2(3, 7));
    EXPECT_EQ(4, finline_value);

    // move initialization of a heap callable
    BinaryOpFunction fheap2(mxtl::move(fheap));
    EXPECT_TRUE(!!fheap2);
    EXPECT_FALSE(!!fheap);
    EXPECT_EQ(10, fheap2(3, 7));
    EXPECT_EQ(3, fheap_value);
    EXPECT_EQ(10, fheap2(3, 7));
    EXPECT_EQ(4, fheap_value);

    // inline mutable lambda
    int fmutinline_value = 0;
    BinaryOpFunction fmutinline([&fmutinline_value, x = 1 ](int a, int b) mutable {
        x *= 2;
        fmutinline_value = x;
        return a + b;
    });
    EXPECT_TRUE(!!fmutinline);
    EXPECT_EQ(10, fmutinline(3, 7));
    EXPECT_EQ(2, fmutinline_value);
    EXPECT_EQ(10, fmutinline(3, 7));
    EXPECT_EQ(4, fmutinline_value);

    // heap-allocated mutable lambda
    int fmutheap_value = 0;
    BinaryOpFunction fmutheap([&fmutheap_value, big = Big(), x = 1 ](int a, int b) mutable {
        x *= 2;
        fmutheap_value = x;
        return a + b;
    });
    EXPECT_TRUE(!!fmutheap);
    EXPECT_EQ(10, fmutheap(3, 7));
    EXPECT_EQ(2, fmutheap_value);
    EXPECT_EQ(10, fmutheap(3, 7));
    EXPECT_EQ(4, fmutheap_value);

    // move assignment of non-null
    BinaryOpFunction fnew([](int a, int b) { return 0; });
    fnew = mxtl::move(finline2);
    EXPECT_TRUE(!!fnew);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(5, finline_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(6, finline_value);

    // move assignment of null
    fnew = mxtl::move(fnull);
    EXPECT_FALSE(!!fnew);

    // callable assignment with operator=
    int fnew_value = 0;
    fnew = [&fnew_value](int a, int b) {
        fnew_value++;
        return a + b;
    };
    EXPECT_TRUE(!!fnew);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(1, fnew_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(2, fnew_value);

    // callable assignment with SetTarget
    fnew.SetTarget([&fnew_value](int a, int b) {
        fnew_value *= 2;
        return a + b;
    });
    EXPECT_TRUE(!!fnew);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(4, fnew_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(8, fnew_value);

    // nullptr assignment
    fnew = nullptr;
    EXPECT_FALSE(!!fnew);

    // swap (currently null)
    fnew.swap(fheap2);
    EXPECT_TRUE(!!fnew);
    EXPECT_FALSE(!!fheap);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(5, fheap_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(6, fheap_value);

    // swap with self
    fnew.swap(fnew);
    EXPECT_TRUE(!!fnew);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(7, fheap_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(8, fheap_value);

    // swap with non-null
    fnew.swap(fmutinline);
    EXPECT_TRUE(!!fmutinline);
    EXPECT_TRUE(!!fnew);
    EXPECT_EQ(10, fmutinline(3, 7));
    EXPECT_EQ(9, fheap_value);
    EXPECT_EQ(10, fmutinline(3, 7));
    EXPECT_EQ(10, fheap_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(8, fmutinline_value);
    EXPECT_EQ(10, fnew(3, 7));
    EXPECT_EQ(16, fmutinline_value);

    // nullptr comparison operators
    EXPECT_TRUE(fnull == nullptr);
    EXPECT_FALSE(fnull != nullptr);
    EXPECT_TRUE(nullptr == fnull);
    EXPECT_FALSE(nullptr != fnull);
    EXPECT_FALSE(fnew == nullptr);
    EXPECT_TRUE(fnew != nullptr);
    EXPECT_FALSE(nullptr == fnew);
    EXPECT_TRUE(nullptr != fnew);

    // alloc checking constructor, inline
    AllocChecker ac1;
    int fcheck_value = 0;
    BinaryOpFunction fcheckinline([&fcheck_value](int a, int b) {
        fcheck_value++;
        return a + b;
    },
                                  &ac1);
    EXPECT_TRUE(!!fcheckinline);
    EXPECT_TRUE(ac1.check());
    EXPECT_EQ(10, fcheckinline(3, 7));
    EXPECT_EQ(1, fcheck_value);

    // alloc checking set target, inline
    AllocChecker ac2;
    fcheckinline.SetTarget([&fcheck_value](int a, int b) {
        fcheck_value *= 3;
        return a + b;
    },
                           &ac2);
    EXPECT_TRUE(!!fcheckinline);
    EXPECT_TRUE(ac2.check());
    EXPECT_EQ(10, fcheckinline(3, 7));
    EXPECT_EQ(3, fcheck_value);

    // alloc checking constructor, heap allocated
    AllocChecker ac3;
    BinaryOpFunction fcheckheap([&fcheck_value, big = Big() ](int a, int b) {
        fcheck_value++;
        return a + b;
    },
                                &ac3);
    EXPECT_TRUE(!!fcheckheap);
    EXPECT_TRUE(ac3.check());
    EXPECT_EQ(10, fcheckheap(3, 7));
    EXPECT_EQ(4, fcheck_value);

    // alloc checking set target, heap allocated
    AllocChecker ac4;
    fcheckheap.SetTarget([&fcheck_value, big = Big() ](int a, int b) {
        fcheck_value *= 3;
        return a + b;
    },
                         &ac4);
    EXPECT_TRUE(!!fcheckheap);
    EXPECT_TRUE(ac4.check());
    EXPECT_EQ(10, fcheckheap(3, 7));
    EXPECT_EQ(12, fcheck_value);

    return true;
}

bool sized_function_size_bounds() {
    auto empty = [] {};
    mxtl::SizedFunction<Closure, sizeof(empty)> fempty(mxtl::move(empty));
    static_assert(sizeof(fempty) >= sizeof(empty), "size bounds");

    auto small = [ x = 1, y = 2 ] {
        (void)x; // suppress unused lambda capture warning
        (void)y;
    };
    mxtl::SizedFunction<Closure, sizeof(small)> fsmall(mxtl::move(small));
    static_assert(sizeof(fsmall) >= sizeof(small), "size bounds");
    fsmall = [] {};

    auto big = [ big = Big(), x = 1 ] { (void)x; };
    mxtl::SizedFunction<Closure, sizeof(big)> fbig(mxtl::move(big));
    static_assert(sizeof(fbig) >= sizeof(big), "size bounds");
    fbig = [ x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };
    fbig = [] {};

    // These statements do compile though the lambda will be copied to the heap
    // when they exceed the inline size.
    fempty = [ x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };
    fsmall = [ big = Big(), x = 1 ] { (void)x; };
    fbig = [ big = Big(), x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };

    return true;
}

bool inline_function_size_bounds() {
    auto empty = [] {};
    mxtl::InlineFunction<Closure, sizeof(empty)> fempty(mxtl::move(empty));
    static_assert(sizeof(fempty) >= sizeof(empty), "size bounds");

    auto small = [ x = 1, y = 2 ] {
        (void)x; // suppress unused lambda capture warning
        (void)y;
    };
    mxtl::InlineFunction<Closure, sizeof(small)> fsmall(mxtl::move(small));
    static_assert(sizeof(fsmall) >= sizeof(small), "size bounds");
    fsmall = [] {};

    auto big = [ big = Big(), x = 1 ] { (void)x; };
    mxtl::InlineFunction<Closure, sizeof(big)> fbig(mxtl::move(big));
    static_assert(sizeof(fbig) >= sizeof(big), "size bounds");
    fbig = [ x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };
    fbig = [] {};

// These statements do not compile because the lambdas are too big to fit.
#if 0
    fempty = [ x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };
    fsmall = [ big = Big(), x = 1 ] { (void)x; };
    fbig = [ big = Big(), x = 1, y = 2 ] {
        (void)x;
        (void)y;
    };
#endif

    return true;
}

bool move_only_argument_and_result() {
    mxtl::unique_ptr<int> arg(new int());
    mxtl::Function<MoveOp> f([](mxtl::unique_ptr<int> value) {
        *value += 1;
        return value;
    });
    arg = f(mxtl::move(arg));
    EXPECT_EQ(1, *arg);
    arg = f(mxtl::move(arg));
    EXPECT_EQ(2, *arg);

    return true;
}

void implicit_construction_helper(mxtl::Closure closure) {}

bool implicit_construction() {
    // ensure we can implicitly construct from nullptr
    implicit_construction_helper(nullptr);

    // ensure we can implicitly construct from a lambda
    implicit_construction_helper([] {});

    return true;
}

// This is the code which is included in <function.h>.
namespace example {
using FoldFunction = mxtl::Function<int(int value, int item)>;

int FoldVector(const mxtl::Vector<int>& in, int value, const FoldFunction& f) {
    for (auto& item : in) {
        value = f(value, item);
    }
    return value;
}

int SumItem(int value, int item) {
    return value + item;
}

int Sum(const mxtl::Vector<int>& in) {
    // bind to a function pointer
    FoldFunction sum(&SumItem);
    return FoldVector(in, 0, sum);
}

int AlternatingSum(const mxtl::Vector<int>& in) {
    // bind to a lambda
    int sign = 1;
    FoldFunction alternating_sum([&sign](int value, int item) {
        value += sign * item;
        sign *= -1;
        return value;
    });
    return FoldVector(in, 0, alternating_sum);
}
} // namespace example

bool example_code() {
    mxtl::Vector<int> in;
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(in.push_back(i));
    }

    EXPECT_EQ(45, example::Sum(in));
    EXPECT_EQ(-5, example::AlternatingSum(in));

    return true;
}

} // namespace

BEGIN_TEST_CASE(function_tests)
RUN_TEST((closure<mxtl::Function<Closure>>))
RUN_TEST((binary_op<mxtl::Function<BinaryOp>>))
RUN_TEST((closure<mxtl::SizedFunction<Closure, 0u>>))
RUN_TEST((binary_op<mxtl::SizedFunction<BinaryOp, 0u>>))
RUN_TEST((closure<mxtl::SizedFunction<Closure, HugeCallableSize>>))
RUN_TEST((binary_op<mxtl::SizedFunction<BinaryOp, HugeCallableSize>>))
RUN_TEST((closure<mxtl::InlineFunction<Closure, HugeCallableSize>>))
RUN_TEST((binary_op<mxtl::InlineFunction<BinaryOp, HugeCallableSize>>))
RUN_TEST(sized_function_size_bounds);
RUN_TEST(inline_function_size_bounds);
RUN_TEST(move_only_argument_and_result);
RUN_TEST(implicit_construction);
RUN_TEST(example_code);
END_TEST_CASE(function_tests)

} // namespace tests
} // namespace mxtl
