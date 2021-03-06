#include "attempt.hpp"

#include "helper.hpp"
#include "problem.hpp"
#include "language.hpp"
#include "contest.hpp"
#include "database.hpp"
#include "judge.hpp"
#include "user.hpp"

#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define MAX_USERS 1000

using namespace std;

static string source(const string& fn) {
  string ans;
  FILE* fp = fopen(fn.c_str(),"rb");
  if (!fp) return "";
  char* buf = new char[(1<<12)+1];
  int sz = fread(buf,1,1<<12,fp);
  buf[sz] = 0;
  ans = buf;
  fclose(fp);
  delete[] buf;
  if(ans == "") ans = " ";
  return ans;
}

namespace Attempt {

static vector<int> att_user[MAX_USERS];

void fix() {
  DB(attempts);
  DB(contests);
  JSON aux;
  attempts.update([&](Database::Document& doc) {
    auto& j = doc.second.obj();
    if (
      j.find("when") != j.end() ||
      j.find("contest") == j.end() ||
      j.find("contest_time") == j.end() ||
      !contests.retrieve(doc.second["contest"],aux)
    ) {
      return false;
    }
    doc.second["when"] = 60*int(doc.second["contest_time"])+Contest::begin(aux);
    return true;
  });
  JSON atts = attempts.retrieve();
  for(const auto &att : atts.arr()){
  	att_user[att("user")].push_back(att("id"));
  }
}

string create(JSON&& att, const vector<uint8_t>& src) {
  // check stuff
  JSON problem = Problem::get_short(att["problem"],att["user"]);
  if (!problem) return "Problem "+att["problem"].str()+" do not exists!";
  JSON setts = Language::settings(att);
  if (!setts) return "Language "+att["language"].str()+" do not exists!";
  if (!Contest::allow_create_attempt(att,problem)) {
    return "Attempts are not allowed right now.";
  }
  // update db
  DB(attempts);
  DB(contests);
  att["after_contest"] = Contest::time(contests.retrieve(int(att("contest"))), int(att("user"))).end >= att("when");
  int id = attempts.create(att);
  att_user[att("user")].push_back(id);
  // save file
  string fn = "attempts/"+tostr(id)+"/";
  system("mkdir -p %soutput",fn.c_str());
  fn += att["problem"].str();
  fn += att["language"].str();
  FILE* fp = fopen(fn.c_str(), "wb");
  fwrite(&src[0],src.size(),1,fp);
  fclose(fp);
  // push
  Judge::push(id);
  // msg
  return "Attempt "+tostr(id)+" received.";
}

JSON get(int id, int user) {
  DB(attempts);
  JSON ans;
  if (!attempts.retrieve(id,ans)) return JSON::null();
  if (user != int(ans["user"])) return JSON::null();
  int cid;
  if (ans("contest").read(cid) && !Contest::get(cid,user)) return JSON::null();
  int pid = ans["problem"];
  string prob = Problem::get_problem_name(pid);
  if (prob == "") return JSON::null();
  ans["id"] = id;
  string ext = ans["language"];
  ans["language"] = Language::settings(ans)["name"];
  ans["problem"] = move(map<string,JSON>{
    {"id"   , pid},
    {"name" , prob}
  });
  ans["source"] = source("attempts/"+tostr(id)+"/"+tostr(pid)+ext);
  ans.erase("ip");
  ans.erase("time");
  ans.erase("memory");
  if (ans["status"] != "judged") ans.erase("verdict");
  return ans;
}

JSON page_profile(int user){
  JSON ans(vector<JSON>{});
  DB(attempts);
  JSON tmp = attempts.retrieve();
  for (auto att : tmp.arr()) {
    if (att("privileged")) continue;
    att.erase("contest");
    att.erase("contest_time");
    att.erase("ip");
    att.erase("language");
    att.erase("memory");
    att.erase("solved_tests");
    att.erase("time");
    att.erase("total_tests");
    att.erase("when");
    ans.push_back(move(att));
  }
  return ans;
}

JSON page(
  int user,
  unsigned p,
  unsigned ps,
  int contest
  ) {
	JSON ans(vector<JSON>{});
	DB(attempts);
	DB(contests);
	JSON att;
	for (int attid : att_user[user]) {
		if(!attempts.retrieve(attid, att)) continue;
		att["id"] = attid;
		int cid;
		bool hasc = att("contest").read(cid);
		if (contest) {
			if (!hasc || cid != contest) continue;
		}
		JSON meucontest = contests.retrieve(cid);
		if(meucontest("qnt_provas")){
            if(!Contest::is_user_allowed(cid, user))
                continue;
        }
		int pid = att["problem"];
		string aux = Problem::get_problem_name(pid);
		if (aux == "") continue;
		att["language"] = Language::settings(att)["name"];
		att["problem"] = move(map<string,JSON>{
				{"id"   , pid},
				{"name" , aux}
				});
		att.erase("ip");
		att.erase("time");
		att.erase("memory");
		if (att["status"] != "judged") {
			att.erase("verdict");
		}
		ans.push_back(move(att));
	}
	if (!ps) {
		p = 0;
		ps = ans.size();
	}
	auto& a = ans.arr();
	unsigned r = (p+1)*ps;
	if (r < a.size()) a.erase(a.begin()+r,a.end());
	a.erase(a.begin(),a.begin()+(p*ps));
	return ans;
}

JSON get_user_contest(int user, int user_id, int contest_id){
    JSON ans(vector<JSON>{});
    if(!user || User::get(user)["turma"] != "Z") return ans;

    DB(attempts);
    DB(problems);
    JSON attempt = attempts.retrieve();

    for(JSON att : attempt.arr()){
        if(att["user"] != user_id) continue;
        int pid = att["problem"];
        JSON tmp;
        if(!problems.retrieve(pid, tmp)) continue;
        if(tmp("contest") != contest_id) continue;
        
        att.erase("status");
        att.erase("language");
        att.erase("memory");
        att.erase("time");
        att.erase("when");
        att.erase("ip");
        att.erase("problem");
        att.erase("verdict");

        ans.push_back(att);
    }
    return ans;
}

JSON getcases(int user, int id){
  JSON ans;
  if(!user) return JSON::null();
  string tmp = User::get(user)["turma"];
  if(tmp != "Z") return JSON::null();

  DB(attempts);
  DB(problems);
  if (!attempts.retrieve(id,ans)) return JSON::null();
  int pid = ans["problem"];
  JSON prob;

  if(!problems.retrieve(pid, prob)) return JSON::null();
  
  ans["id"] = id;
  string ext = ans["language"];
  ans["language"] = Language::settings(ans)["name"];
  ans["problem"] = move(map<string,JSON>{
    {"id"   , pid},
    {"name" , prob["name"]}
  });
  ans["source"] = source("attempts/"+tostr(id)+"/"+tostr(pid)+ext);
  ans.erase("ip");
  ans.erase("time");
  ans.erase("memory");

  if (ans["status"] != "judged") ans.erase("verdict");

  ans["tests"] = JSON(vector<JSON>());
  string path = "attempts/"+tostr(id);

// for each input file
  string dn = "problems/"+tostr(pid);
  DIR* dir = opendir((dn+"/input").c_str());
  for (dirent* ent = readdir(dir); ent; ent = readdir(dir)) {
    string fn = ent->d_name;
    string ifn = dn+"/input/"+fn;
    
    // check if dirent is regular file
    struct stat stt;
    stat(ifn.c_str(),&stt);
    if (!S_ISREG(stt.st_mode)) continue;
    
    string ofn = path+"/output/"+fn; //output obtido
    string sfn = dn+"/output/"+fn; //output esperado
    string input = source(ifn);
    string outexpected = source(sfn);
    string outrecieved = source(ofn);

    if(outrecieved != ""){
      // ans["tests"].push_back(vector<JSON>{input, outrecieved, outexpected});
      JSON tmp;
      tmp["input"] = input;
      tmp["outrecieved"] = outrecieved;
      tmp["outexpected"] = outexpected;
      ans["tests"].push_back(tmp);
    }
  }
  closedir(dir);

  return ans;
}

} // namespace Attempt
