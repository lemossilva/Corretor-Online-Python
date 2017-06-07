#ifndef AHO
#define AHO
#include "trie.hpp"
#include <vector>

using namespace std;

class Aho : public Trie{
	vector<int> fail;
	vector<int> outputlink;
	vector<bool> vis;
	vector<vector<int> > pos;
	vector<int> last_state;
    vector<int> strsize;

public:
	void build_failures();

	void back(int ptr, int id);

	pair<long long, long long> run(string s);

	int has_output(int s) const;

	int anda(int s, char c) const;

	bool was_visited(int id) const;

	Aho(const vector<string> &v);
};

#endif
