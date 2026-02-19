//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_NON_ALLOCATING_STABLE_SORT_HEADER_INCLUDED
#define CHOC_NON_ALLOCATING_STABLE_SORT_HEADER_INCLUDED

#include <algorithm>

namespace choc::sorting
{

//==============================================================================
/// Annoyingly the std::stable_sort() function can allocate, which makes it useless
/// in realtime code, so this is a stable sort algorithm that's realtime-safe, with
/// decent performance. It's based on a hybrid insertion/merge sort algorithm.
template <typename Iterator, typename Comparator>
void stable_sort (Iterator start, Iterator end, const Comparator& isLess);

/// Annoyingly the std::stable_sort() function can allocate, which makes it useless
/// in realtime code, so this is a stable sort algorithm that's realtime-safe, with
/// decent performance. It's based on a hybrid insertion/merge sort algorithm.
template <typename Iterator>
void stable_sort (Iterator beg, Iterator end);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

template <typename Iterator, typename Comparator>
struct StableSortHelpers
{
    using ValueType = typename std::iterator_traits<Iterator>::value_type;
    using DiffType  = typename std::iterator_traits<Iterator>::difference_type;
    static constexpr DiffType insertionSortLength = 32;

    static void sort (const Iterator start, const Iterator end, const Comparator& isLess)
    {
        const auto length = std::distance (start, end);

        if (length > 1)
        {
            performInsertionSorts (start, end, length, isLess);

            if (length > insertionSortLength)
                performMergeSorts (start, end, length, isLess);
        }
    }

private:
    static void performInsertionSorts (Iterator start, Iterator end, DiffType length, const Comparator& isLess)
    {
        DiffType total = 0;
        auto i = start;

        for (;;)
        {
            total += insertionSortLength;

            if (total > length)
            {
                if (std::distance (i, end) > 1)
                    insertionSort (i, end, isLess);

                break;
            }

            auto next = i + insertionSortLength;
            insertionSort (i, next, isLess);
            i = next;
        }
    }

    static void insertionSort (Iterator start, Iterator end, const Comparator& isLess)
    {
        for (auto i = start + 1; i != end; ++i)
        {
            alignas(ValueType) char tempSpace[sizeof(ValueType)];
            auto val = reinterpret_cast<ValueType*> (tempSpace);
            new (val) ValueType (std::move (*i));
            auto j = i;

            while (j != start && isLess (*val, *(j - 1)))
            {
                new (std::addressof (*j)) ValueType (std::move (*(j - 1)));
                --j;
            }

            new (std::addressof (*j)) ValueType (std::move (*val));
        }
    }

    static void performMergeSorts (const Iterator start, const Iterator end, const DiffType length, const Comparator& isLess)
    {
        auto currentStart = start + insertionSortLength;
        auto currentSize = length > 2 * insertionSortLength ? insertionSortLength : (length - insertionSortLength);
        auto lastSize = insertionSortLength;

        while (currentSize >= insertionSortLength && currentStart != end)
        {
            if (lastSize == currentSize)
            {
                rotateMerge (currentStart - lastSize, currentStart, currentStart + currentSize, isLess);
                currentSize += lastSize;
                currentStart -= lastSize;
                lastSize = getLastSize (start, currentStart);
            }
            else
            {
                currentStart += currentSize;
                lastSize = currentSize;
                currentSize = std::min (std::distance (currentStart, end), insertionSortLength);
            }
        }

        if (currentStart == end)
        {
            currentStart -= lastSize;
            lastSize = getLastSize (start, currentStart);
        }

        while (currentStart != start)
        {
            rotateMerge (currentStart - lastSize, currentStart, end, isLess);
            currentStart -= lastSize;
            lastSize = getLastSize (start, currentStart);
        }
    }

    static void rotateMerge (const Iterator start, const Iterator middle, const Iterator end, const Comparator& isLess)
    {
        if (auto length = std::distance (start, middle))
        {
            for (auto i = end; length > 0;)
            {
                innerRotateMerge (start + length / 2, start + length, i, isLess, i);
                length /= 2;
            }
        }
    }

    static void innerRotateMerge (Iterator start, Iterator middle, Iterator end, const Comparator& isLess, Iterator& out)
    {
        if (const auto length = std::distance (start, middle))
        {
            Iterator i;

            if (findItem (middle, end, *start, isLess, i))
            {
                --i;

                while (i != (middle - 1) && ! isLess (*start, *i) && ! isLess (*i, *start))
                    --i;

                ++i;
            }

            auto pos = std::distance (middle, i);
            std::rotate (start, middle, i);
            out = start + pos;

            if (length > 1)
            {
                Iterator tempOut = {};
                innerRotateMerge (out + length / 2, out + length, end, isLess, tempOut);
                innerRotateMerge (out + 1, out + length / 2, tempOut, isLess, tempOut);
            }
        }
    }

    static bool findItem (Iterator start, Iterator end, const ValueType& target, const Comparator& isLess, Iterator& found)
    {
        if (auto length = std::distance (start, end))
        {
            found = start;

            for (auto n = length; n > 0;)
            {
                auto step = n / 2;
                auto i = found + step;

                if (isLess (*i, target))
                {
                    n -= step + 1;
                    found = ++i;
                }
                else
                {
                    n = step;
                }
            }

            return found != end && ! isLess (*found, target) && ! isLess (target, *found);
        }

        found = end;
        return false;
    }

    static DiffType getLastSize (Iterator start, Iterator end)
    {
        DiffType result = 0;

        if (auto length = std::distance (start, end))
        {
            for (;;)
            {
                result = insertionSortLength;

                while (result * 2 <= length)
                    result *= 2;

                length -= result;

                if (length <= 0)
                    break;
            }
        }

        return result;
    }
};

template <typename Iterator, typename Comparator>
void stable_sort (Iterator start, Iterator end, const Comparator& isLess)
{
    StableSortHelpers<Iterator, Comparator>::sort (start, end, isLess);
}

template <typename Iterator>
void stable_sort (Iterator start, Iterator end)
{
    choc::sorting::stable_sort (start, end, [] (auto a, auto b) { return a < b; });
}

} // namespace choc::sorting

#endif // CHOC_NON_ALLOCATING_STABLE_SORT_HEADER_INCLUDED
