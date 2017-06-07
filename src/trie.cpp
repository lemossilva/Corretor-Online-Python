#include "trie.hpp"

using namespace std;

Trie::Trie() : trie(1), final(1){}

const map<char, int> &Trie::getchilds(int state) const{
    return trie[state];
}

int Trie::insert_word(const string& s){
    int ptr = 0;
    for(char c : s){
        if(!trie[ptr].count(c)){
            trie[ptr][c] = trie.size();
            trie.resize(trie.size()+1);
            final.resize(final.size()+1);
        }
        ptr = trie[ptr][c];
    }
    final[ptr] = true;
    return ptr;
}

