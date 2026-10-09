#include "framework.h"
#include "Renderer/Next/DrawSort.h"

namespace TEN::Renderer::Next
{
    constexpr auto DIGIT_BITS  = 8;
    constexpr auto DIGIT_COUNT = 1 << DIGIT_BITS;
    constexpr auto KEY_BITS    = 64;

    void SortDrawKeys(std::vector<DrawSortKey>& keys, std::vector<DrawSortKey>& scratch)
    {
        if (keys.size() < 2)
            return;

        scratch.resize(keys.size());
        auto* source = &keys;
        auto* destination = &scratch;

        for (int shift = 0; shift < KEY_BITS; shift += DIGIT_BITS)
        {
            int counts[DIGIT_COUNT] = {};
            for (const auto& key : *source)
                counts[(key.Key >> shift) & (DIGIT_COUNT - 1)]++;

            if (counts[((*source)[0].Key >> shift) & (DIGIT_COUNT - 1)] == (int)source->size())
                continue;

            int offset = 0;
            for (int& count : counts)
            {
                int digitCount = count;
                count = offset;
                offset += digitCount;
            }

            for (const auto& key : *source)
                (*destination)[counts[(key.Key >> shift) & (DIGIT_COUNT - 1)]++] = key;

            std::swap(source, destination);
        }

        if (source != &keys)
            keys.swap(scratch);
    }
}
