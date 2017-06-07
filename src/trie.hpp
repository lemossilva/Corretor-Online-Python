#ifndef TRIE
#define TRIE
#include <vector>
#include <queue>
#include <map>

using namespace std;

class Trie{
    protected:
        vector<map<char, int> > trie;
        vector<bool> final;
    public:

        Trie();
        inline bool has_edge(int state, char c) const{
            return trie[state].count(c);	
        }

        inline int change_state(int state, char c) const{
            return has_edge(state, c) ? trie[state].at(c) : 0;
        }

        inline bool is_final(int state) const{
            return state < final.size() && final[state];
        }

        inline size_t size() const{
            return trie.size();
        }

        const map<char, int> &getchilds(int state) const;

        int insert_word(const string& s);
};

#endif
