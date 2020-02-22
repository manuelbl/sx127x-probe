#include "rolling_average.h"

#include <algorithm>
#include <numeric>

void RollingAvergage::AddValue(int32_t newValue)
{
    buffer[currentIndex] = newValue;
    currentIndex++;
    if (currentIndex >= buffer.size())
        currentIndex = 0;
    currentSize = std::min(currentSize + 1, BUFFER_SIZE);
}

int32_t RollingAvergage::Mean() const
{
    auto sum = std::accumulate(begin(buffer), begin(buffer) + currentSize, (int32_t)0);
    return sum / currentSize;
}

int32_t RollingAvergage::Variance() const
{
    auto mean = Mean();
    auto diffToMeanSqare = [mean](int32_t accumulator, int32_t val) { return accumulator + ((val - mean) * (val - mean)); };
    auto sumsquare = std::accumulate(begin(buffer), begin(buffer) + currentSize, (int32_t)0, diffToMeanSqare);

    return sumsquare / currentSize;
}
