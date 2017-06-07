#include "aho.hpp"
#include "bit.hpp"
#include "trie.hpp"

using namespace std;

void Aho::build_failures(){
    fail.assign(size(), 0);

    queue<int> q;

    for(pair<const char, const int> p : getchilds(0)){
        fail[ p.second ] = 0;
        q.push(p.second);
    }

    while(!q.empty()){
        int u = q.front(); q.pop();

        for(pair<const char, const int> p : getchilds(u)){
            char c = p.first;
            int v = p.second, ptr = fail[u];

            while(ptr && !has_edge(ptr, c)) ptr = fail[ptr];

            if(has_edge(ptr, c)) ptr = change_state(ptr, c);
            fail[v] = ptr;

            if(is_final(ptr)) outputlink[v] = ptr;
            else if(outputlink[ptr]) outputlink[v] = outputlink[ptr];

            q.push(v);
        }
    }
}

void Aho::back(int ptr, int id){
    if(!ptr) return;
    vis[ptr] = 1;
    pos[ptr].push_back(id+1);
    back(outputlink[ptr], id);
}

pair<long long, long long> Aho::run(string s){
    int ptr = 0;
    for(int i = 0; i < s.size(); i++){
        if(!has_edge(ptr, s[i])){
            int nptr = ptr;
            while(nptr && !has_edge(nptr, s[i])) nptr = fail[nptr];

            if(has_edge(nptr, s[i])) nptr = change_state(nptr, s[i]);
            trie[ptr][ s[i] ] = nptr;
        }

        ptr = change_state(ptr, s[i]);
        if(is_final(ptr)) back(ptr, i);
        else back(outputlink[ptr], i);
    }

    Bit ft(s.size()+1);
    for(int i = 0; i < last_state.size(); i++){
        for(int j = pos[ last_state[i] ].size()-1; j >= 0; j--){
            int id = pos[ last_state[i] ][j];
            ft.update(id, ft.query(id-strsize[i])+1);
        }
    }
    
    return {ft.query(s.size()+1), last_state.size()};
}

int Aho::has_output(int s) const{
    return outputlink[s];
}

int Aho::anda(int s, char c)const{
    while(s && !has_edge(s, c)) s = fail[s];
    if(has_edge(s, c)) s = change_state(s, c);
    return s;
}

bool Aho::was_visited(int id) const{
    return vis[ last_state[id] ];
}

Aho::Aho(const vector<string> &v){
    Trie();
    last_state.assign(v.size(), 0);
    strsize.assign(v.size(), 0);
    for(int i = 0; i < v.size(); i++)
        last_state[i] = insert_word(v[i]), strsize[i] = v[i].size();
    vis.assign(size(), 0);
    pos.assign(size(), vector<int>());
    outputlink.assign(size(), 0);
    build_failures();
}

