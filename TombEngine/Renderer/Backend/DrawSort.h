#pragma once

namespace TEN::Renderer::Backend
{
    // Draw index with the state key it is sorted by.
    struct DrawSortKey
    {
        unsigned long long Key   = 0;
        int                Index = 0;
    };

    // Stable least significant digit radix sort on 8 bit digits; digits equal in every key are skipped.
    // The scratch vector keeps its capacity across frames.
    void SortDrawKeys(std::vector<DrawSortKey>& keys, std::vector<DrawSortKey>& scratch);
}
