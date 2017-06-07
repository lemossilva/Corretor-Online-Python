#ifndef BIT
#define BIT
#include <vector>

class Bit{
    std::vector<int> ft;
    int n;

public:
    Bit(int);
    int query(int x) const;
    void update(int x, int val);
};

#endif
