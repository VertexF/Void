#ifndef FOUNDATION_ALGORITHM_ALGORITHM_HDR
#define FOUNDATION_ALGORITHM_ALGORITHM_HDR

// ============================================================================
// Engine - Core Module
// Algorithm - Freestanding Range Algorithms
// ============================================================================
// Zero C library dependency. Replaces <algorithm> for freestanding builds.
// Originated from ELT algorithm.h — adapted to Engine conventions.

#include <Platform.hpp>
#include <Utility/Move.hpp>
#include <Containers/Pair.hpp>
#include <Memory/IAllocator.hpp>
#include <initializer_list>
// Note: Cannot include Array.h here (circular dependency — Array.h includes Algorithm.h)

namespace Engine::Algorithm {

// ============================================================================
// Tie (lexicographic key helper)
// ============================================================================

namespace Detail {

template<typename... Ts>
struct TieKey;

template<>
struct TieKey<> {
    constexpr TieKey() noexcept = default;
    [[nodiscard]] constexpr bool operator<(const TieKey&) const noexcept { return false; }
    [[nodiscard]] constexpr bool operator==(const TieKey&) const noexcept { return true; }
};

template<typename T, typename... Rest>
struct TieKey<T, Rest...> {
    const T& Head;
    TieKey<Rest...> Tail;

    constexpr TieKey(const T& head, const Rest&... rest) noexcept
        : Head(head), Tail(rest...) {}

    [[nodiscard]] constexpr bool operator<(const TieKey& other) const noexcept {
        if (Head < other.Head) {
            return true;
        }
        if (other.Head < Head) {
            return false;
        }
        return Tail < other.Tail;
    }

    [[nodiscard]] constexpr bool operator==(const TieKey& other) const noexcept {
        return Head == other.Head && Tail == other.Tail;
    }
};

} // namespace Detail

template<typename... Ts>
[[nodiscard]] constexpr Detail::TieKey<Ts...> Tie(const Ts&... values) noexcept {
    return Detail::TieKey<Ts...>(values...);
}

// ============================================================================
// Clamp
// ============================================================================

template<typename T>
constexpr const T& Clamp(const T& Value, const T& Lo, const T& Hi) noexcept {
    return Value < Lo ? Lo : (Value > Hi ? Hi : Value);
}

// ============================================================================
// Fill
// ============================================================================

template<typename ForwardIt, typename T>
constexpr void Fill(ForwardIt First, ForwardIt Last, const T& Value) {
    for (; First != Last; ++First) {
        *First = Value;
    }
}

template<typename OutputIt, typename SizeType, typename T>
constexpr OutputIt FillN(OutputIt First, SizeType Count, const T& Value) {
    for (SizeType I = 0; I < Count; ++I) {
        *First++ = Value;
    }
    return First;
}

// ============================================================================
// Copy
// ============================================================================

template<typename InputIt, typename OutputIt>
constexpr OutputIt Copy(InputIt First, InputIt Last, OutputIt DFirst) {
    for (; First != Last; ++First, ++DFirst) {
        *DFirst = *First;
    }
    return DFirst;
}

template<typename InputIt, typename OutputIt, typename Pred>
constexpr OutputIt CopyIf(InputIt First, InputIt Last, OutputIt DFirst, Pred P) {
    for (; First != Last; ++First) {
        if (P(*First)) {
            *DFirst++ = *First;
        }
    }
    return DFirst;
}

template<typename InputIt, typename SizeType, typename OutputIt>
constexpr OutputIt CopyN(InputIt First, SizeType Count, OutputIt DFirst) {
    for (SizeType I = 0; I < Count; ++I) {
        *DFirst++ = *First++;
    }
    return DFirst;
}

// ============================================================================
// Move (range)
// ============================================================================

template<typename InputIt, typename OutputIt>
constexpr OutputIt MoveRange(InputIt First, InputIt Last, OutputIt DFirst) {
    for (; First != Last; ++First, ++DFirst) {
        *DFirst = Move(*First);
    }
    return DFirst;
}

// ============================================================================
// Find
// ============================================================================

template<typename InputIt, typename T>
constexpr InputIt Find(InputIt First, InputIt Last, const T& Value) {
    for (; First != Last; ++First) {
        if (*First == Value) return First;
    }
    return Last;
}

template<typename InputIt, typename Pred>
constexpr InputIt FindIf(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (P(*First)) return First;
    }
    return Last;
}

template<typename InputIt, typename Pred>
constexpr InputIt FindIfNot(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (!P(*First)) return First;
    }
    return Last;
}

// ============================================================================
// Count
// ============================================================================

template<typename InputIt, typename T>
constexpr usize Count(InputIt First, InputIt Last, const T& Value) {
    usize Result = 0;
    for (; First != Last; ++First) {
        if (*First == Value) ++Result;
    }
    return Result;
}

template<typename InputIt, typename Pred>
constexpr usize CountIf(InputIt First, InputIt Last, Pred P) {
    usize Result = 0;
    for (; First != Last; ++First) {
        if (P(*First)) ++Result;
    }
    return Result;
}

// ============================================================================
// All/Any/None
// ============================================================================

template<typename InputIt, typename Pred>
constexpr bool AllOf(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (!P(*First)) return false;
    }
    return true;
}

template<typename InputIt, typename Pred>
constexpr bool AnyOf(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (P(*First)) return true;
    }
    return false;
}

template<typename InputIt, typename Pred>
constexpr bool NoneOf(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (P(*First)) return false;
    }
    return true;
}

// ============================================================================
// Equal
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr bool Equal(InputIt1 First1, InputIt1 Last1, InputIt2 First2) {
    for (; First1 != Last1; ++First1, ++First2) {
        if (!(*First1 == *First2)) return false;
    }
    return true;
}

// ============================================================================
// Transform
// ============================================================================

template<typename InputIt, typename OutputIt, typename UnaryOp>
constexpr OutputIt Transform(InputIt First, InputIt Last, OutputIt DFirst, UnaryOp Op) {
    for (; First != Last; ++First, ++DFirst) {
        *DFirst = Op(*First);
    }
    return DFirst;
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename BinaryOp>
constexpr OutputIt Transform(InputIt1 First1, InputIt1 Last1,
                              InputIt2 First2, OutputIt DFirst, BinaryOp Op) {
    for (; First1 != Last1; ++First1, ++First2, ++DFirst) {
        *DFirst = Op(*First1, *First2);
    }
    return DFirst;
}

// ============================================================================
// Reverse
// ============================================================================

template<typename BidirIt>
constexpr void Reverse(BidirIt First, BidirIt Last) {
    while (First != Last && First != --Last) {
        auto Temp = Move(*First);
        *First = Move(*Last);
        *Last = Move(Temp);
        ++First;
    }
}

// ============================================================================
// Lower/Upper Bound (Binary Search)
// ============================================================================

template<typename ForwardIt, typename T>
constexpr ForwardIt LowerBound(ForwardIt First, ForwardIt Last, const T& Value) {
    auto Count = Last - First;
    while (Count > 0) {
        auto Step = Count / 2;
        auto Mid = First + Step;
        if (*Mid < Value) {
            First = Mid + 1;
            Count -= Step + 1;
        } else {
            Count = Step;
        }
    }
    return First;
}

template<typename ForwardIt, typename T>
constexpr ForwardIt UpperBound(ForwardIt First, ForwardIt Last, const T& Value) {
    auto Count = Last - First;
    while (Count > 0) {
        auto Step = Count / 2;
        auto Mid = First + Step;
        if (!(Value < *Mid)) {
            First = Mid + 1;
            Count -= Step + 1;
        } else {
            Count = Step;
        }
    }
    return First;
}

template<typename ForwardIt, typename T>
constexpr bool BinarySearch(ForwardIt First, ForwardIt Last, const T& Value) {
    auto It = LowerBound(First, Last, Value);
    return It != Last && !(Value < *It);
}

// ============================================================================
// Unique
// ============================================================================

template<typename ForwardIt>
constexpr ForwardIt Unique(ForwardIt First, ForwardIt Last) {
    if (First == Last) return Last;
    ForwardIt Result = First;
    while (++First != Last) {
        if (!(*Result == *First)) {
            *(++Result) = static_cast<RemoveReference<decltype(*First)>&&>(*First);
        }
    }
    return ++Result;
}

// ============================================================================
// Generate
// ============================================================================

template<typename ForwardIt, typename Generator>
constexpr void Generate(ForwardIt First, ForwardIt Last, Generator Gen) {
    for (; First != Last; ++First) {
        *First = Gen();
    }
}

// ============================================================================
// ForEach
// ============================================================================

template<typename InputIt, typename Fn>
constexpr Fn ForEach(InputIt First, InputIt Last, Fn F) {
    for (; First != Last; ++First) {
        F(*First);
    }
    return F;
}

// ============================================================================
// Min/Max Element
// ============================================================================

template<typename ForwardIt>
constexpr ForwardIt MinElement(ForwardIt First, ForwardIt Last) {
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (*First < *Result) Result = First;
    }
    return Result;
}

template<typename ForwardIt, typename Compare>
constexpr ForwardIt MinElement(ForwardIt First, ForwardIt Last, Compare Comp) {
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (Comp(*First, *Result)) Result = First;
    }
    return Result;
}

template<typename ForwardIt>
constexpr ForwardIt MaxElement(ForwardIt First, ForwardIt Last) {
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (*Result < *First) Result = First;
    }
    return Result;
}

template<typename ForwardIt, typename Compare>
constexpr ForwardIt MaxElement(ForwardIt First, ForwardIt Last, Compare Comp) {
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (Comp(*Result, *First)) Result = First;
    }
    return Result;
}

// ============================================================================
// Partition
// ============================================================================

template<typename ForwardIt, typename Pred>
constexpr ForwardIt Partition(ForwardIt First, ForwardIt Last, Pred P) {
    First = FindIfNot(First, Last, P);
    if (First == Last) return First;

    for (ForwardIt It = First; ++It != Last;) {
        if (P(*It)) {
            // Swap *First and *It
            auto Temp = static_cast<RemoveReference<decltype(*First)>&&>(*First);
            *First = static_cast<RemoveReference<decltype(*It)>&&>(*It);
            *It = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
            ++First;
        }
    }
    return First;
}

// ============================================================================
// Rotate
// ============================================================================

template<typename ForwardIt>
constexpr ForwardIt Rotate(ForwardIt First, ForwardIt Middle, ForwardIt Last) {
    if (First == Middle) return Last;
    if (Middle == Last) return First;

    ForwardIt Read = Middle;
    ForwardIt Write = First;

    while (Read != Last) {
        if (Write == Middle) Middle = Read;
        auto Temp = static_cast<RemoveReference<decltype(*Write)>&&>(*Write);
        *Write = static_cast<RemoveReference<decltype(*Read)>&&>(*Read);
        *Read = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
        ++Write;
        ++Read;
    }

    Rotate(Write, Middle, Last);
    return Write;
}

// ============================================================================
// Lexicographical Compare
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr bool LexicographicalCompare(InputIt1 First1, InputIt1 Last1,
                                       InputIt2 First2, InputIt2 Last2) {
    for (; First1 != Last1 && First2 != Last2; ++First1, ++First2) {
        if (*First1 < *First2) return true;
        if (*First2 < *First1) return false;
    }
    return First1 == Last1 && First2 != Last2;
}

// ============================================================================
// Mismatch
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr Pair<InputIt1, InputIt2> Mismatch(InputIt1 First1, InputIt1 Last1,
                                             InputIt2 First2) {
    while (First1 != Last1 && *First1 == *First2) {
        ++First1;
        ++First2;
    }
    return {First1, First2};
}

// ============================================================================
// Sort — Introsort (Quicksort + Heapsort + Insertion sort)
// ============================================================================
// Production-quality sort: O(n log n) worst case.
// - Quicksort for average case
// - Heapsort fallback when recursion depth exceeds 2*log2(n)
// - Insertion sort for small partitions (<=16 elements)

namespace Detail {

template<typename RandomIt>
constexpr void InsertionSort(RandomIt First, RandomIt Last) {
    if (First == Last) return;
    for (RandomIt I = First + 1; I != Last; ++I) {
        auto Key = static_cast<RemoveReference<decltype(*I)>&&>(*I);
        RandomIt J = I;
        while (J > First && *(J - 1) > Key) {
            *J = static_cast<RemoveReference<decltype(*(J-1))>&&>(*(J - 1));
            --J;
        }
        *J = static_cast<RemoveReference<decltype(Key)>&&>(Key);
    }
}

template<typename RandomIt, typename Compare>
constexpr void InsertionSort(RandomIt First, RandomIt Last, Compare Comp) {
    if (First == Last) return;
    for (RandomIt I = First + 1; I != Last; ++I) {
        auto Key = static_cast<RemoveReference<decltype(*I)>&&>(*I);
        RandomIt J = I;
        while (J > First && Comp(Key, *(J - 1))) {
            *J = static_cast<RemoveReference<decltype(*(J-1))>&&>(*(J - 1));
            --J;
        }
        *J = static_cast<RemoveReference<decltype(Key)>&&>(Key);
    }
}

template<typename RandomIt>
constexpr void SiftDown(RandomIt First, decltype(First - First) Start,
                         decltype(First - First) End) {
    auto Root = Start;
    while (Root * 2 + 1 <= End) {
        auto Child = Root * 2 + 1;
        if (Child + 1 <= End && *(First + Child) < *(First + Child + 1)) {
            ++Child;
        }
        if (*(First + Root) < *(First + Child)) {
            auto Temp = static_cast<RemoveReference<decltype(*(First + Root))>&&>(*(First + Root));
            *(First + Root) = static_cast<RemoveReference<decltype(*(First + Child))>&&>(*(First + Child));
            *(First + Child) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
            Root = Child;
        } else {
            return;
        }
    }
}

template<typename RandomIt>
constexpr void HeapSort(RandomIt First, RandomIt Last) {
    auto Count = Last - First;
    if (Count < 2) return;

    // Build max heap
    for (auto I = Count / 2 - 1; I >= 0; --I) {
        SiftDown(First, I, Count - 1);
    }

    // Extract elements
    for (auto I = Count - 1; I > 0; --I) {
        auto Temp = static_cast<RemoveReference<decltype(*First)>&&>(*First);
        *First = static_cast<RemoveReference<decltype(*(First + I))>&&>(*(First + I));
        *(First + I) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
        SiftDown(First, decltype(I)(0), I - 1);
    }
}

template<typename RandomIt>
constexpr RandomIt MedianOfThree(RandomIt A, RandomIt B, RandomIt C) {
    if (*A < *B) {
        if (*B < *C) return B;      // a < b < c
        if (*A < *C) return C;      // a < c <= b
        return A;                    // c <= a < b
    }
    if (*A < *C) return A;          // b <= a < c
    if (*B < *C) return C;          // b < c <= a
    return B;                        // c <= b <= a
}

template<typename RandomIt>
constexpr void IntrosortImpl(RandomIt First, RandomIt Last, int DepthLimit) {
    while (Last - First > 16) {
        if (DepthLimit == 0) {
            HeapSort(First, Last);
            return;
        }
        --DepthLimit;

        // Median-of-three pivot selection
        auto Mid = First + (Last - First) / 2;
        auto PivotIt = MedianOfThree(First, Mid, Last - 1);

        // Move pivot to end
        auto Temp = static_cast<RemoveReference<decltype(*PivotIt)>&&>(*PivotIt);
        *PivotIt = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);

        // Partition
        auto PivotPos = First;
        for (auto It = First; It != Last - 1; ++It) {
            if (*It < *(Last - 1)) {
                auto T2 = static_cast<RemoveReference<decltype(*PivotPos)>&&>(*PivotPos);
                *PivotPos = static_cast<RemoveReference<decltype(*It)>&&>(*It);
                *It = static_cast<RemoveReference<decltype(T2)>&&>(T2);
                ++PivotPos;
            }
        }

        // Move pivot to final position
        auto T3 = static_cast<RemoveReference<decltype(*PivotPos)>&&>(*PivotPos);
        *PivotPos = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(T3)>&&>(T3);

        // Recurse on smaller partition, iterate on larger
        if (PivotPos - First < Last - (PivotPos + 1)) {
            IntrosortImpl(First, PivotPos, DepthLimit);
            First = PivotPos + 1;
        } else {
            IntrosortImpl(PivotPos + 1, Last, DepthLimit);
            Last = PivotPos;
        }
    }
    InsertionSort(First, Last);
}

constexpr int Log2Floor(usize N) {
    int Result = 0;
    while (N > 1) { N >>= 1; ++Result; }
    return Result;
}

// ============================================================================
// Comparator-aware versions (full introsort, not insertion-sort fallback)
// ============================================================================

template<typename RandomIt, typename Compare>
constexpr RandomIt MedianOfThreeComp(RandomIt A, RandomIt B, RandomIt C, Compare& Comp) {
    if (Comp(*A, *B)) {
        if (Comp(*B, *C)) return B;
        if (Comp(*A, *C)) return C;
        return A;
    }
    if (Comp(*A, *C)) return A;
    if (Comp(*B, *C)) return C;
    return B;
}

template<typename RandomIt, typename Compare>
constexpr void SiftDownComp(RandomIt First, decltype(First - First) Start,
                             decltype(First - First) End, Compare& Comp) {
    auto Root = Start;
    while (Root * 2 + 1 <= End) {
        auto Child = Root * 2 + 1;
        if (Child + 1 <= End && Comp(*(First + Child), *(First + Child + 1))) {
            ++Child;
        }
        if (Comp(*(First + Root), *(First + Child))) {
            auto Temp = static_cast<RemoveReference<decltype(*(First + Root))>&&>(*(First + Root));
            *(First + Root) = static_cast<RemoveReference<decltype(*(First + Child))>&&>(*(First + Child));
            *(First + Child) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
            Root = Child;
        } else {
            return;
        }
    }
}

template<typename RandomIt, typename Compare>
constexpr void HeapSortComp(RandomIt First, RandomIt Last, Compare& Comp) {
    auto Count = Last - First;
    if (Count < 2) return;

    // Build max-heap
    for (auto I = Count / 2 - 1; I >= 0; --I) {
        SiftDownComp(First, I, Count - 1, Comp);
    }

    // Extract
    for (auto End = Count - 1; End > 0; --End) {
        auto Temp = static_cast<RemoveReference<decltype(*First)>&&>(*First);
        *First = static_cast<RemoveReference<decltype(*(First + End))>&&>(*(First + End));
        *(First + End) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
        SiftDownComp(First, decltype(End){0}, End - 1, Comp);
    }
}

template<typename RandomIt, typename Compare>
constexpr void IntrosortImplComp(RandomIt First, RandomIt Last, int DepthLimit, Compare& Comp) {
    while (Last - First > 16) {
        if (DepthLimit == 0) {
            HeapSortComp(First, Last, Comp);
            return;
        }
        --DepthLimit;

        // Median-of-three pivot selection
        auto Mid = First + (Last - First) / 2;
        auto PivotIt = MedianOfThreeComp(First, Mid, Last - 1, Comp);

        // Move pivot to end
        auto Temp = static_cast<RemoveReference<decltype(*PivotIt)>&&>(*PivotIt);
        *PivotIt = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);

        // Partition
        auto PivotPos = First;
        for (auto It = First; It != Last - 1; ++It) {
            if (Comp(*It, *(Last - 1))) {
                auto T2 = static_cast<RemoveReference<decltype(*PivotPos)>&&>(*PivotPos);
                *PivotPos = static_cast<RemoveReference<decltype(*It)>&&>(*It);
                *It = static_cast<RemoveReference<decltype(T2)>&&>(T2);
                ++PivotPos;
            }
        }

        // Move pivot to final position
        auto T3 = static_cast<RemoveReference<decltype(*PivotPos)>&&>(*PivotPos);
        *PivotPos = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(T3)>&&>(T3);

        // Recurse on smaller partition, iterate on larger
        if (PivotPos - First < Last - (PivotPos + 1)) {
            IntrosortImplComp(First, PivotPos, DepthLimit, Comp);
            First = PivotPos + 1;
        } else {
            IntrosortImplComp(PivotPos + 1, Last, DepthLimit, Comp);
            Last = PivotPos;
        }
    }
    InsertionSort(First, Last, Comp);
}

} // namespace Detail

/// @brief Introsort — O(n log n) worst case, cache-friendly, no allocations
template<typename RandomIt>
constexpr void Sort(RandomIt First, RandomIt Last) {
    auto Count = Last - First;
    if (Count < 2) return;
    Detail::IntrosortImpl(First, Last, Detail::Log2Floor(static_cast<usize>(Count)) * 2);
}

/// @brief Introsort with custom comparator — O(n log n) worst case
template<typename RandomIt, typename Compare>
constexpr void Sort(RandomIt First, RandomIt Last, Compare Comp) {
    auto Count = Last - First;
    if (Count < 2) return;
    Detail::IntrosortImplComp(First, Last, Detail::Log2Floor(static_cast<usize>(Count)) * 2, Comp);
}

// ============================================================================
// IsSorted / IsSortedUntil
// ============================================================================

template<typename ForwardIt>
constexpr bool IsSorted(ForwardIt First, ForwardIt Last) {
    if (First == Last) return true;
    ForwardIt Prev = First;
    ++First;
    for (; First != Last; ++First, ++Prev) {
        if (*First < *Prev) return false;
    }
    return true;
}

template<typename ForwardIt>
constexpr ForwardIt IsSortedUntil(ForwardIt First, ForwardIt Last) {
    if (First == Last) return Last;
    ForwardIt Prev = First;
    ++First;
    for (; First != Last; ++First, ++Prev) {
        if (*First < *Prev) return First;
    }
    return Last;
}

// ============================================================================
// Accumulate / Reduce
// ============================================================================

template<typename InputIt, typename T>
constexpr T Accumulate(InputIt First, InputIt Last, T Init) {
    for (; First != Last; ++First) {
        Init = Init + *First;
    }
    return Init;
}

template<typename InputIt, typename T, typename BinaryOp>
constexpr T Accumulate(InputIt First, InputIt Last, T Init, BinaryOp Op) {
    for (; First != Last; ++First) {
        Init = Op(Init, *First);
    }
    return Init;
}

// ============================================================================
// NthElement (partial sort for selection)
// ============================================================================

template<typename RandomIt>
constexpr void NthElement(RandomIt First, RandomIt Nth, RandomIt Last) {
    // Quickselect — O(n) average
    while (Last - First > 1) {
        // Partition around last element
        auto Pivot = *(Last - 1);
        auto StoreIdx = First;
        for (auto It = First; It != Last - 1; ++It) {
            if (*It < Pivot) {
                auto Temp = static_cast<RemoveReference<decltype(*StoreIdx)>&&>(*StoreIdx);
                *StoreIdx = static_cast<RemoveReference<decltype(*It)>&&>(*It);
                *It = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
                ++StoreIdx;
            }
        }
        auto Temp = static_cast<RemoveReference<decltype(*StoreIdx)>&&>(*StoreIdx);
        *StoreIdx = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);

        if (StoreIdx == Nth) return;
        if (Nth < StoreIdx) {
            Last = StoreIdx;
        } else {
            First = StoreIdx + 1;
        }
    }
}

template<typename RandomIt, typename Comp>
constexpr void NthElement(RandomIt First, RandomIt Nth, RandomIt Last, Comp comp) {
    while (Last - First > 1) {
        auto Pivot = *(Last - 1);
        auto StoreIdx = First;
        for (auto It = First; It != Last - 1; ++It) {
            if (comp(*It, Pivot)) {
                auto Temp = static_cast<RemoveReference<decltype(*StoreIdx)>&&>(*StoreIdx);
                *StoreIdx = static_cast<RemoveReference<decltype(*It)>&&>(*It);
                *It = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
                ++StoreIdx;
            }
        }
        auto Temp = static_cast<RemoveReference<decltype(*StoreIdx)>&&>(*StoreIdx);
        *StoreIdx = static_cast<RemoveReference<decltype(*(Last - 1))>&&>(*(Last - 1));
        *(Last - 1) = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);

        if (StoreIdx == Nth) return;
        if (Nth < StoreIdx) {
            Last = StoreIdx;
        } else {
            First = StoreIdx + 1;
        }
    }
}

// ============================================================================
// Swap Ranges
// ============================================================================

template<typename ForwardIt1, typename ForwardIt2>
constexpr ForwardIt2 SwapRanges(ForwardIt1 First1, ForwardIt1 Last1, ForwardIt2 First2) {
    for (; First1 != Last1; ++First1, ++First2) {
        auto Temp = static_cast<RemoveReference<decltype(*First1)>&&>(*First1);
        *First1 = static_cast<RemoveReference<decltype(*First2)>&&>(*First2);
        *First2 = static_cast<RemoveReference<decltype(Temp)>&&>(Temp);
    }
    return First2;
}

// ============================================================================
// Remove / RemoveIf (logical removal — shifts elements, returns new end)
// ============================================================================

template<typename ForwardIt, typename T>
constexpr ForwardIt Remove(ForwardIt First, ForwardIt Last, const T& Value) {
    First = Find(First, Last, Value);
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (!(*First == Value)) {
            *Result = static_cast<RemoveReference<decltype(*First)>&&>(*First);
            ++Result;
        }
    }
    return Result;
}

template<typename ForwardIt, typename Pred>
constexpr ForwardIt RemoveIf(ForwardIt First, ForwardIt Last, Pred P) {
    First = FindIf(First, Last, P);
    if (First == Last) return Last;
    ForwardIt Result = First;
    ++First;
    for (; First != Last; ++First) {
        if (!P(*First)) {
            *Result = static_cast<RemoveReference<decltype(*First)>&&>(*First);
            ++Result;
        }
    }
    return Result;
}

// ============================================================================
// Replace / ReplaceIf
// ============================================================================

template<typename ForwardIt, typename T>
constexpr void Replace(ForwardIt First, ForwardIt Last,
                        const T& OldValue, const T& NewValue) {
    for (; First != Last; ++First) {
        if (*First == OldValue) *First = NewValue;
    }
}

template<typename ForwardIt, typename Pred, typename T>
constexpr void ReplaceIf(ForwardIt First, ForwardIt Last, Pred P, const T& NewValue) {
    for (; First != Last; ++First) {
        if (P(*First)) *First = NewValue;
    }
}

template<typename T>
constexpr const T& Max(const T& A, const T& B) noexcept {
    return A < B ? B : A;
}

template<typename T>
constexpr const T& Min(const T& A, const T& B) noexcept {
    return A < B ? A : B;
}

// ============================================================================
// CopyBackward — copies range [First, Last) into [..., DLast) going backwards
// ============================================================================

template<typename BidirIt1, typename BidirIt2>
constexpr BidirIt2 CopyBackward(BidirIt1 First, BidirIt1 Last, BidirIt2 DLast) {
    while (First != Last) {
        *--DLast = *--Last;
    }
    return DLast;
}

// ============================================================================
// MoveBackward — moves range [First, Last) into [..., DLast) going backwards
// ============================================================================

template<typename BidirIt1, typename BidirIt2>
constexpr BidirIt2 MoveBackward(BidirIt1 First, BidirIt1 Last, BidirIt2 DLast) {
    while (First != Last) {
        *--DLast = static_cast<RemoveReference<decltype(*--Last)>&&>(*Last);
    }
    return DLast;
}

// ============================================================================
// GenerateN
// ============================================================================

template<typename OutputIt, typename SizeType, typename Generator>
constexpr OutputIt GenerateN(OutputIt First, SizeType Count, Generator Gen) {
    for (SizeType I = 0; I < Count; ++I) {
        *First++ = Gen();
    }
    return First;
}

// ============================================================================
// ReverseCopy — copies range in reverse order without modifying source
// ============================================================================

template<typename BidirIt, typename OutputIt>
constexpr OutputIt ReverseCopy(BidirIt First, BidirIt Last, OutputIt DFirst) {
    while (First != Last) {
        *DFirst++ = *--Last;
    }
    return DFirst;
}

// ============================================================================
// Iterator Utilities
// ============================================================================

/// @brief Advance iterator by N positions
template<typename InputIt, typename Distance>
constexpr void Advance(InputIt& It, Distance N) {
    It += N; // Random-access optimization; TODO: input-iterator fallback
}

/// @brief Distance between two iterators
template<typename InputIt>
constexpr auto Distance(InputIt First, InputIt Last) -> decltype(Last - First) {
    return Last - First; // Random-access optimization
}

/// @brief Return iterator advanced by N (default 1)
template<typename InputIt>
constexpr InputIt Next(InputIt It, decltype(It - It) N = 1) {
    return It + N;
}

/// @brief Return iterator retreated by N (default 1)
template<typename BidirIt>
constexpr BidirIt Prev(BidirIt It, decltype(It - It) N = 1) {
    return It - N;
}

// ---- StableSort (merge sort) ----

namespace Detail {

template<typename It, typename Comp>
void MergeSortImpl(It first, It last, Comp comp) {
    const auto n = last - first;
    if (n <= 1) return;
    if (n <= 16) {
        // Insertion sort for small ranges
        for (auto i = first + 1; i != last; ++i) {
            auto key = Move(*i);
            auto j = i;
            while (j != first && comp(key, *(j - 1))) {
                *j = Move(*(j - 1));
                --j;
            }
            *j = Move(key);
        }
        return;
    }
    auto mid = first + n / 2;
    MergeSortImpl(first, mid, comp);
    MergeSortImpl(mid, last, comp);

    // In-place merge using temporary buffer on stack for small, heap for large
    using T = decltype(*first);
    const auto leftN = mid - first;
    const auto rightN = last - mid;

    // Simple merge into temp + copy back using raw heap allocation
    using ValT = RemoveReference<T>;
    ValT* tmp = static_cast<ValT*>(
        Engine::Memory::GetDefaultAllocator().Allocate(static_cast<usize>(n) * sizeof(ValT), alignof(ValT)));
    usize tmpSize = 0;
    auto l = first, r = mid;
    while (l != mid && r != last) {
        if (comp(*r, *l)) {
            ::new (tmp + tmpSize) ValT(Move(*r)); ++r; ++tmpSize;
        } else {
            ::new (tmp + tmpSize) ValT(Move(*l)); ++l; ++tmpSize;
        }
    }
    while (l != mid) { ::new (tmp + tmpSize) ValT(Move(*l)); ++l; ++tmpSize; }
    while (r != last) { ::new (tmp + tmpSize) ValT(Move(*r)); ++r; ++tmpSize; }
    for (usize i = 0; i < tmpSize; ++i) {
        *(first + i) = Move(tmp[i]);
        tmp[i].~ValT();
    }
    Engine::Memory::GetDefaultAllocator().Free(tmp);
}

} // namespace Detail

template<typename It>
void StableSort(It first, It last) {
    Detail::MergeSortImpl(first, last, [](const auto& a, const auto& b) { return a < b; });
}

template<typename It, typename Comp>
void StableSort(It first, It last, Comp comp) {
    Detail::MergeSortImpl(first, last, comp);
}

// ============================================================================
// Additional Algorithms
// ============================================================================

/// PartialSort — sort first N elements
template<typename It, typename Comp>
void PartialSort(It first, It middle, It last, Comp comp) {
    // Build max-heap on [first, middle)
    auto n = middle - first;
    for (auto i = n / 2 - 1; i >= 0; --i) {
        // Sift down
        auto parent = i;
        while (true) {
            auto child = 2 * parent + 1;
            if (child >= n) break;
            if (child + 1 < n && comp(*(first + child), *(first + child + 1))) ++child;
            if (!comp(*(first + parent), *(first + child))) break;
            auto tmp = static_cast<decltype(*first)&&>(*(first + parent));
            *(first + parent) = static_cast<decltype(*first)&&>(*(first + child));
            *(first + child) = static_cast<decltype(*first)&&>(tmp);
            parent = child;
        }
    }
    for (auto it = middle; it != last; ++it) {
        if (comp(*it, *first)) {
            auto tmp = static_cast<decltype(*first)&&>(*it);
            *it = static_cast<decltype(*first)&&>(*first);
            *first = static_cast<decltype(*first)&&>(tmp);
            // Sift down root
            decltype(n) parent = 0;
            while (true) {
                auto child = 2 * parent + 1;
                if (child >= n) break;
                if (child + 1 < n && comp(*(first + child), *(first + child + 1))) ++child;
                if (!comp(*(first + parent), *(first + child))) break;
                auto t2 = static_cast<decltype(*first)&&>(*(first + parent));
                *(first + parent) = static_cast<decltype(*first)&&>(*(first + child));
                *(first + child) = static_cast<decltype(*first)&&>(t2);
                parent = child;
            }
        }
    }
    Sort(first, middle, comp);
}

template<typename It>
void PartialSort(It first, It middle, It last) {
    PartialSort(first, middle, last, [](const auto& a, const auto& b) { return a < b; });
}

/// AdjacentFind — find first pair of adjacent equal elements
template<typename It>
It AdjacentFind(It first, It last) {
    if (first == last) return last;
    auto prev = first;
    ++first;
    while (first != last) {
        if (*prev == *first) return prev;
        prev = first;
        ++first;
    }
    return last;
}

template<typename It, typename Pred>
It AdjacentFind(It first, It last, Pred pred) {
    if (first == last) return last;
    auto prev = first;
    ++first;
    while (first != last) {
        if (pred(*prev, *first)) return prev;
        prev = first;
        ++first;
    }
    return last;
}

/// Shuffle — Fisher-Yates shuffle
template<typename It, typename RNG>
void Shuffle(It first, It last, RNG&& rng) {
    auto n = last - first;
    for (auto i = n - 1; i > 0; --i) {
        auto j = rng() % (i + 1);
        auto tmp = static_cast<decltype(*first)&&>(*(first + i));
        *(first + i) = static_cast<decltype(*first)&&>(*(first + j));
        *(first + j) = static_cast<decltype(*first)&&>(tmp);
    }
}

/// PushHeap / PopHeap — binary max-heap operations
template<typename It, typename Comp>
void PushHeap(It first, It last, Comp comp) {
    auto i = (last - first) - 1;
    while (i > 0) {
        auto parent = (i - 1) / 2;
        if (!comp(*(first + parent), *(first + i))) break;
        auto tmp = static_cast<decltype(*first)&&>(*(first + i));
        *(first + i) = static_cast<decltype(*first)&&>(*(first + parent));
        *(first + parent) = static_cast<decltype(*first)&&>(tmp);
        i = parent;
    }
}

template<typename It>
void PushHeap(It first, It last) {
    PushHeap(first, last, [](const auto& a, const auto& b) { return a < b; });
}

template<typename It, typename Comp>
void PopHeap(It first, It last, Comp comp) {
    auto n = last - first;
    if (n <= 1) return;
    --last;
    auto tmp = static_cast<decltype(*first)&&>(*first);
    *first = static_cast<decltype(*first)&&>(*last);
    *last = static_cast<decltype(*first)&&>(tmp);
    --n;
    // Sift down
    decltype(n) parent = 0;
    while (true) {
        auto child = 2 * parent + 1;
        if (child >= n) break;
        if (child + 1 < n && comp(*(first + child), *(first + child + 1))) ++child;
        if (!comp(*(first + parent), *(first + child))) break;
        auto t2 = static_cast<decltype(*first)&&>(*(first + parent));
        *(first + parent) = static_cast<decltype(*first)&&>(*(first + child));
        *(first + child) = static_cast<decltype(*first)&&>(t2);
        parent = child;
    }
}

template<typename It>
void PopHeap(It first, It last) {
    PopHeap(first, last, [](const auto& a, const auto& b) { return a < b; });
}

/// EqualTo / Greater / Less — function objects
template<typename T = void> struct EqualTo {
    [[nodiscard]] constexpr bool operator()(const T& a, const T& b) const noexcept { return a == b; }
};
template<> struct EqualTo<void> {
    template<typename T, typename U>
    [[nodiscard]] constexpr bool operator()(const T& a, const U& b) const noexcept { return a == b; }
};

/// Search — find subsequence
template<typename It1, typename It2>
It1 Search(It1 first, It1 last, It2 s_first, It2 s_last) {
    for (;; ++first) {
        auto it = first;
        for (auto s_it = s_first;; ++it, ++s_it) {
            if (s_it == s_last) return first;
            if (it == last) return last;
            if (!(*it == *s_it)) break;
        }
    }
}

/// IsPermutation — check if range is a permutation of another
template<typename It1, typename It2>
bool IsPermutation(It1 first1, It1 last1, It2 first2) {
    // Skip common prefix
    auto it2 = first2;
    for (auto it1 = first1; it1 != last1; ++it1, ++it2) {
        if (!(*it1 == *it2)) break;
        first1 = it1; ++first1;
        first2 = it2; ++first2;
    }
    auto last2 = first2;
    for (auto it = first1; it != last1; ++it) ++last2;
    // Brute force remaining
    for (auto it = first1; it != last1; ++it) {
        auto count1 = 0, count2 = 0;
        for (auto j = first1; j != last1; ++j) if (*j == *it) ++count1;
        for (auto j = first2; j != last2; ++j) if (*j == *it) ++count2;
        if (count1 != count2) return false;
    }
    return true;
}

// ============================================================================
// Comparator-aware LowerBound / UpperBound / BinarySearch
// ============================================================================

template<typename ForwardIt, typename T, typename Compare>
constexpr ForwardIt LowerBound(ForwardIt First, ForwardIt Last, const T& Value, Compare Comp) {
    auto Count = Last - First;
    while (Count > 0) {
        auto Step = Count / 2;
        auto Mid = First + Step;
        if (Comp(*Mid, Value)) {
            First = Mid + 1;
            Count -= Step + 1;
        } else {
            Count = Step;
        }
    }
    return First;
}

template<typename ForwardIt, typename T, typename Compare>
constexpr ForwardIt UpperBound(ForwardIt First, ForwardIt Last, const T& Value, Compare Comp) {
    auto Count = Last - First;
    while (Count > 0) {
        auto Step = Count / 2;
        auto Mid = First + Step;
        if (!Comp(Value, *Mid)) {
            First = Mid + 1;
            Count -= Step + 1;
        } else {
            Count = Step;
        }
    }
    return First;
}

template<typename ForwardIt, typename T, typename Compare>
constexpr bool BinarySearch(ForwardIt First, ForwardIt Last, const T& Value, Compare Comp) {
    auto It = LowerBound(First, Last, Value, Comp);
    return It != Last && !Comp(Value, *It);
}

// ============================================================================
// EqualRange
// ============================================================================

template<typename ForwardIt, typename T>
constexpr Pair<ForwardIt, ForwardIt> EqualRange(ForwardIt First, ForwardIt Last, const T& Value) {
    return {LowerBound(First, Last, Value), UpperBound(First, Last, Value)};
}

template<typename ForwardIt, typename T, typename Compare>
constexpr Pair<ForwardIt, ForwardIt> EqualRange(ForwardIt First, ForwardIt Last, const T& Value, Compare Comp) {
    return {LowerBound(First, Last, Value, Comp), UpperBound(First, Last, Value, Comp)};
}

// ============================================================================
// Heap Operations (bulk)
// ============================================================================

template<typename RandomIt, typename Compare>
constexpr void MakeHeap(RandomIt First, RandomIt Last, Compare Comp) {
    auto Count = Last - First;
    if (Count < 2) return;
    for (auto I = Count / 2 - 1; I >= 0; --I) {
        // Sift down
        auto Parent = I;
        while (true) {
            auto Child = 2 * Parent + 1;
            if (Child >= Count) break;
            if (Child + 1 < Count && Comp(*(First + Child), *(First + Child + 1))) ++Child;
            if (!Comp(*(First + Parent), *(First + Child))) break;
            auto Temp = Move(*(First + Parent));
            *(First + Parent) = Move(*(First + Child));
            *(First + Child) = Move(Temp);
            Parent = Child;
        }
    }
}

template<typename RandomIt>
constexpr void MakeHeap(RandomIt First, RandomIt Last) {
    MakeHeap(First, Last, [](const auto& A, const auto& B) { return A < B; });
}

template<typename RandomIt, typename Compare>
constexpr void SortHeap(RandomIt First, RandomIt Last, Compare Comp) {
    while (Last - First > 1) {
        PopHeap(First, Last, Comp);
        --Last;
    }
}

template<typename RandomIt>
constexpr void SortHeap(RandomIt First, RandomIt Last) {
    SortHeap(First, Last, [](const auto& A, const auto& B) { return A < B; });
}

template<typename RandomIt, typename Compare>
constexpr bool IsHeap(RandomIt First, RandomIt Last, Compare Comp) {
    auto Count = Last - First;
    for (decltype(Count) I = 0; I < Count; ++I) {
        auto Left = 2 * I + 1;
        auto Right = 2 * I + 2;
        if (Left < Count && Comp(*(First + I), *(First + Left))) return false;
        if (Right < Count && Comp(*(First + I), *(First + Right))) return false;
    }
    return true;
}

template<typename RandomIt>
constexpr bool IsHeap(RandomIt First, RandomIt Last) {
    return IsHeap(First, Last, [](const auto& A, const auto& B) { return A < B; });
}

template<typename RandomIt, typename Compare>
constexpr RandomIt IsHeapUntil(RandomIt First, RandomIt Last, Compare Comp) {
    auto Count = Last - First;
    for (decltype(Count) I = 0; I < Count; ++I) {
        auto Left = 2 * I + 1;
        auto Right = 2 * I + 2;
        if (Left < Count && Comp(*(First + I), *(First + Left))) return First + I;
        if (Right < Count && Comp(*(First + I), *(First + Right))) return First + I;
    }
    return Last;
}

template<typename RandomIt>
constexpr RandomIt IsHeapUntil(RandomIt First, RandomIt Last) {
    return IsHeapUntil(First, Last, [](const auto& A, const auto& B) { return A < B; });
}

// ============================================================================
// Set Operations (on sorted ranges)
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr bool Includes(InputIt1 First1, InputIt1 Last1, InputIt2 First2, InputIt2 Last2) {
    for (; First2 != Last2; ++First1) {
        if (First1 == Last1 || *First2 < *First1) return false;
        if (!(*First1 < *First2)) ++First2;
    }
    return true;
}

template<typename InputIt1, typename InputIt2, typename Compare>
constexpr bool Includes(InputIt1 First1, InputIt1 Last1, InputIt2 First2, InputIt2 Last2, Compare Comp) {
    for (; First2 != Last2; ++First1) {
        if (First1 == Last1 || Comp(*First2, *First1)) return false;
        if (!Comp(*First1, *First2)) ++First2;
    }
    return true;
}

template<typename InputIt1, typename InputIt2, typename OutputIt>
constexpr OutputIt SetUnion(InputIt1 First1, InputIt1 Last1,
                             InputIt2 First2, InputIt2 Last2, OutputIt DFirst) {
    for (; First1 != Last1; ++DFirst) {
        if (First2 == Last2) return Copy(First1, Last1, DFirst);
        if (*First2 < *First1) {
            *DFirst = *First2++;
        } else {
            *DFirst = *First1;
            if (!(*First1 < *First2)) ++First2;
            ++First1;
        }
    }
    return Copy(First2, Last2, DFirst);
}

template<typename InputIt1, typename InputIt2, typename OutputIt>
constexpr OutputIt SetIntersection(InputIt1 First1, InputIt1 Last1,
                                    InputIt2 First2, InputIt2 Last2, OutputIt DFirst) {
    while (First1 != Last1 && First2 != Last2) {
        if (*First1 < *First2) {
            ++First1;
        } else {
            if (!(*First2 < *First1)) {
                *DFirst++ = *First1++;
            }
            ++First2;
        }
    }
    return DFirst;
}

template<typename InputIt1, typename InputIt2, typename OutputIt>
constexpr OutputIt SetDifference(InputIt1 First1, InputIt1 Last1,
                                  InputIt2 First2, InputIt2 Last2, OutputIt DFirst) {
    while (First1 != Last1) {
        if (First2 == Last2) return Copy(First1, Last1, DFirst);
        if (*First1 < *First2) {
            *DFirst++ = *First1++;
        } else {
            if (!(*First2 < *First1)) ++First1;
            ++First2;
        }
    }
    return DFirst;
}

template<typename InputIt1, typename InputIt2, typename OutputIt>
constexpr OutputIt SetSymmetricDifference(InputIt1 First1, InputIt1 Last1,
                                           InputIt2 First2, InputIt2 Last2, OutputIt DFirst) {
    while (First1 != Last1) {
        if (First2 == Last2) return Copy(First1, Last1, DFirst);
        if (*First1 < *First2) {
            *DFirst++ = *First1++;
        } else {
            if (*First2 < *First1) {
                *DFirst++ = *First2;
            } else {
                ++First1;
            }
            ++First2;
        }
    }
    return Copy(First2, Last2, DFirst);
}

// ============================================================================
// Merge
// ============================================================================

template<typename InputIt1, typename InputIt2, typename OutputIt>
constexpr OutputIt Merge(InputIt1 First1, InputIt1 Last1,
                          InputIt2 First2, InputIt2 Last2, OutputIt DFirst) {
    while (First1 != Last1 && First2 != Last2) {
        if (*First2 < *First1) {
            *DFirst++ = *First2++;
        } else {
            *DFirst++ = *First1++;
        }
    }
    if (First1 != Last1) return Copy(First1, Last1, DFirst);
    return Copy(First2, Last2, DFirst);
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
constexpr OutputIt Merge(InputIt1 First1, InputIt1 Last1,
                          InputIt2 First2, InputIt2 Last2, OutputIt DFirst, Compare Comp) {
    while (First1 != Last1 && First2 != Last2) {
        if (Comp(*First2, *First1)) {
            *DFirst++ = *First2++;
        } else {
            *DFirst++ = *First1++;
        }
    }
    if (First1 != Last1) return Copy(First1, Last1, DFirst);
    return Copy(First2, Last2, DFirst);
}

template<typename BidirIt>
constexpr void InplaceMerge(BidirIt First, BidirIt Middle, BidirIt Last) {
    if (First == Middle || Middle == Last) return;
    // Rotation-based in-place merge (O(n log n), no allocation)
    for (auto It = Middle; It != Last; ++It) {
        auto Value = Move(*It);
        auto Pos = UpperBound(First, It, Value);
        // Rotate elements: shift [Pos, It) right by 1
        for (auto J = It; J != Pos; --J) {
            *J = Move(*(J - 1));
        }
        *Pos = Move(Value);
    }
}

template<typename BidirIt, typename Compare>
constexpr void InplaceMerge(BidirIt First, BidirIt Middle, BidirIt Last, Compare Comp) {
    if (First == Middle || Middle == Last) return;
    for (auto It = Middle; It != Last; ++It) {
        auto Value = Move(*It);
        auto Pos = UpperBound(First, It, Value, Comp);
        for (auto J = It; J != Pos; --J) {
            *J = Move(*(J - 1));
        }
        *Pos = Move(Value);
    }
}

// ============================================================================
// Partition Operations
// ============================================================================

template<typename ForwardIt, typename Pred>
constexpr ForwardIt PartitionPoint(ForwardIt First, ForwardIt Last, Pred P) {
    auto N = Last - First;
    while (N > 0) {
        auto Step = N / 2;
        auto Mid = First + Step;
        if (P(*Mid)) {
            First = Mid + 1;
            N -= Step + 1;
        } else {
            N = Step;
        }
    }
    return First;
}

template<typename InputIt, typename Pred>
constexpr bool IsPartitioned(InputIt First, InputIt Last, Pred P) {
    for (; First != Last; ++First) {
        if (!P(*First)) break;
    }
    for (; First != Last; ++First) {
        if (P(*First)) return false;
    }
    return true;
}

template<typename InputIt, typename OutputIt1, typename OutputIt2, typename Pred>
constexpr Pair<OutputIt1, OutputIt2> PartitionCopy(InputIt First, InputIt Last,
                                                    OutputIt1 DTrue, OutputIt2 DFalse, Pred P) {
    for (; First != Last; ++First) {
        if (P(*First)) {
            *DTrue++ = *First;
        } else {
            *DFalse++ = *First;
        }
    }
    return {DTrue, DFalse};
}

// ============================================================================
// Permutation Operations
// ============================================================================

template<typename BidirIt>
constexpr bool NextPermutation(BidirIt First, BidirIt Last) {
    if (First == Last) return false;
    auto I = Last;
    --I;
    if (First == I) return false;

    while (true) {
        auto IP1 = I;
        --I;
        if (*I < *IP1) {
            auto J = Last;
            --J;
            while (!(*I < *J)) --J;
            auto Temp = Move(*I); *I = Move(*J); *J = Move(Temp);
            Reverse(IP1, Last);
            return true;
        }
        if (I == First) {
            Reverse(First, Last);
            return false;
        }
    }
}

template<typename BidirIt>
constexpr bool PrevPermutation(BidirIt First, BidirIt Last) {
    if (First == Last) return false;
    auto I = Last;
    --I;
    if (First == I) return false;

    while (true) {
        auto IP1 = I;
        --I;
        if (*IP1 < *I) {
            auto J = Last;
            --J;
            while (!(*J < *I)) --J;
            auto Temp = Move(*I); *I = Move(*J); *J = Move(Temp);
            Reverse(IP1, Last);
            return true;
        }
        if (I == First) {
            Reverse(First, Last);
            return false;
        }
    }
}

// ============================================================================
// Search Algorithms
// ============================================================================

template<typename ForwardIt1, typename ForwardIt2>
constexpr ForwardIt1 FindFirstOf(ForwardIt1 First, ForwardIt1 Last,
                                  ForwardIt2 SFirst, ForwardIt2 SLast) {
    for (; First != Last; ++First) {
        for (auto It = SFirst; It != SLast; ++It) {
            if (*First == *It) return First;
        }
    }
    return Last;
}

template<typename ForwardIt1, typename ForwardIt2>
constexpr ForwardIt1 FindEnd(ForwardIt1 First, ForwardIt1 Last,
                              ForwardIt2 SFirst, ForwardIt2 SLast) {
    if (SFirst == SLast) return Last;
    ForwardIt1 Result = Last;
    while (true) {
        auto NewResult = Search(First, Last, SFirst, SLast);
        if (NewResult == Last) break;
        Result = NewResult;
        First = Result;
        ++First;
    }
    return Result;
}

template<typename ForwardIt, typename T>
constexpr ForwardIt SearchN(ForwardIt First, ForwardIt Last, usize Count, const T& Value) {
    if (Count == 0) return First;
    ForwardIt Result = First;
    usize Found = 0;
    for (; First != Last; ++First) {
        if (*First == Value) {
            if (Found == 0) Result = First;
            if (++Found == Count) return Result;
        } else {
            Found = 0;
        }
    }
    return Last;
}

// ============================================================================
// Sample — reservoir sampling
// ============================================================================

template<typename InputIt, typename OutputIt, typename RNG>
constexpr OutputIt Sample(InputIt First, InputIt Last, OutputIt Out, usize N, RNG&& Rng) {
    usize I = 0;
    for (; First != Last && I < N; ++First, ++I) {
        *(Out + I) = *First;
    }
    for (; First != Last; ++First, ++I) {
        auto R = Rng() % (I + 1);
        if (R < N) {
            *(Out + R) = *First;
        }
    }
    return Out + Min(N, I);
}

// ============================================================================
// Min / Max / MinMax with std::initializer_list
// ============================================================================

template<typename T>
constexpr T Min(std::initializer_list<T> IList) {
    auto It = IList.begin();
    T Result = *It;
    ++It;
    for (; It != IList.end(); ++It) {
        if (*It < Result) Result = *It;
    }
    return Result;
}

template<typename T>
constexpr T Max(std::initializer_list<T> IList) {
    auto It = IList.begin();
    T Result = *It;
    ++It;
    for (; It != IList.end(); ++It) {
        if (Result < *It) Result = *It;
    }
    return Result;
}

template<typename T>
constexpr Pair<T, T> MinMax(const T& A, const T& B) {
    return A < B ? Pair<T, T>{A, B} : Pair<T, T>{B, A};
}

template<typename T>
constexpr Pair<T, T> MinMax(std::initializer_list<T> IList) {
    auto It = IList.begin();
    T MinVal = *It;
    T MaxVal = *It;
    ++It;
    for (; It != IList.end(); ++It) {
        if (*It < MinVal) MinVal = *It;
        if (MaxVal < *It) MaxVal = *It;
    }
    return {MinVal, MaxVal};
}

// ============================================================================
// Clamp with comparator
// ============================================================================

template<typename T, typename Compare>
constexpr const T& Clamp(const T& Value, const T& Lo, const T& Hi, Compare Comp) {
    return Comp(Value, Lo) ? Lo : Comp(Hi, Value) ? Hi : Value;
}

// ============================================================================
// Mismatch (with two-range version)
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr Pair<InputIt1, InputIt2> Mismatch(InputIt1 First1, InputIt1 Last1,
                                             InputIt2 First2, InputIt2 Last2) {
    while (First1 != Last1 && First2 != Last2 && *First1 == *First2) {
        ++First1;
        ++First2;
    }
    return {First1, First2};
}

// ============================================================================
// Equal (with two-range version)
// ============================================================================

template<typename InputIt1, typename InputIt2>
constexpr bool Equal(InputIt1 First1, InputIt1 Last1, InputIt2 First2, InputIt2 Last2) {
    while (First1 != Last1 && First2 != Last2) {
        if (!(*First1 == *First2)) return false;
        ++First1;
        ++First2;
    }
    return First1 == Last1 && First2 == Last2;
}

// ============================================================================
// RotateCopy
// ============================================================================

template<typename ForwardIt, typename OutputIt>
constexpr OutputIt RotateCopy(ForwardIt First, ForwardIt Middle, ForwardIt Last, OutputIt DFirst) {
    DFirst = Copy(Middle, Last, DFirst);
    return Copy(First, Middle, DFirst);
}

} // namespace Engine::Algorithm

#endif // !ENGINE_FOUNDATION_ALGORITHM_ALGORITHM_HPP
