#include "bit.hpp"

Bit::Bit(int sz) : n(sz), ft(sz+1, 0) {}

int Bit::query(int x) const{
    int ans = 0;
    for(; x > 0; x -= x&-x)
        ans = std::max(ans, ft[x]);
    return ans;
}

void Bit::update(int x, int val){
    for(; x <= n; x += x&-x)
        ft[x] = std::max(ft[x], val);
}
