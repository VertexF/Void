#ifndef FOUNDATION_ALGORITHM_SORT_HPP
#define FOUNDATION_ALGORITHM_SORT_HPP

namespace Engine::Algorithm {

template<typename It, typename Compare>
constexpr void sort(It first, It last, Compare comp)
{
    Sort(first, last, comp);
}

template<typename It>
constexpr void sort(It first, It last)
{
    Sort(first, last);
}

template<typename It, typename Compare>
constexpr void stable_sort(It first, It last, Compare comp)
{
    StableSort(first, last, comp);
}

template<typename It>
constexpr void stable_sort(It first, It last)
{
    StableSort(first, last);
}

} // namespace Engine::Algorithm
#endif // !FOUNDATION_ALGORITHM_SORT_HPP