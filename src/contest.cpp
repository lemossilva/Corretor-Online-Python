#include <cmath>
#include <algorithm>

#include "contest.hpp"

#include "helper.hpp"
#include "problem.hpp"
#include "attempt.hpp"
#include "user.hpp"

using namespace std;

static bool isjudge(int user, const JSON& contest) {
  JSON tmp = contest("judges");
  if (!tmp.isarr()) return false;
  auto& a = tmp.arr();
  auto i = lower_bound(a.begin(),a.end(),user,std::less<int>())-a.begin();
  return i < a.size() && int(a[i]) == user;
}

static JSON list_problems(const JSON& contest, int user) {
  JSON probs;
  if(contest("qnt_provas")){
    int prova = (user % int(contest("qnt_provas"))) + 1;
    probs = contest("prova", tostr(prova));
  }
  else probs = contest("problems");

  JSON ans(vector<JSON>{}), tmp;
  for (int pid : probs.arr()) {
    tmp = Problem::get_short(pid,user);
    if (!tmp) continue;
    ans.push_back(move(tmp));
  }
  return ans;
}

time_t inner_begin(const JSON& start) {
  int Y = start("year");
  int M = start("month");
  int D = start("day");
  int h = start("hour");
  int m = start("minute");
  time_t tmp = ::time(nullptr);
  tm ti;
  localtime_r(&tmp,&ti);
  ti.tm_year = Y - 1900;
  ti.tm_mon  = M - 1;
  ti.tm_mday = D;
  ti.tm_hour = h;
  ti.tm_min  = m;
  ti.tm_sec  = 0;
  return mktime(&ti);
}

namespace Contest {

void fix() {
  DB(contests);
  DB(problems);
  JSON aux;
  contests.update([&](Database::Document& doc) {
    if (doc.second("judges").isarr()) {
      auto& a = doc.second["judges"].arr();
      sort(a.begin(),a.end(),std::less<int>());
    }
    auto& ps = doc.second["problems"];
    if (!ps.isarr()) {
      // ps = JSON(vector<JSON>{});
      return true;
    }
    for (int id : ps.arr()) if (problems.retrieve(id,aux)) {
      aux["contest"] = doc.first;
      problems.update(id,move(aux));
    }
    return true;
  });
  problems.update([&](Database::Document& doc) {
    int cid;
    if (
      !doc.second["contest"].read(cid) || !contests.retrieve(cid,aux)) {
      doc.second.erase("contest");
      return true;
    }
    if(aux("qnt_provas")){
      int sz = int(aux("qnt_provas"));
      for(int i = 1; i <= sz; i++){
        for(int id : aux["prova"][tostr(i)].arr())
          if(id == doc.first) return true;
      }
    }
    else if(aux("problems"))
      for (int id : aux["problems"].arr()) if (id == doc.first) return true;
    doc.second.erase("contest");
    return true;
  });
}

Time time(const JSON& contest, int user) {
  Time ans;

  if(user){
    string turma = User::get(user)["turma"].str();
    if(contest("start", turma)) ans.begin = inner_begin(contest("start", turma));
    else ans.begin = 0;
  }
  else ans.begin = 0;
  ans.end = ans.begin + 60*int(contest("duration"));

  return ans;
}

time_t begin(const JSON& contest) {
  int Y = contest("start","year");
  int M = contest("start","month");
  int D = contest("start","day");
  int h = contest("start","hour");
  int m = contest("start","minute");
  time_t tmp = ::time(nullptr);
  tm ti;
  localtime_r(&tmp,&ti);
  ti.tm_year = Y - 1900;
  ti.tm_mon  = M - 1;
  ti.tm_mday = D;
  ti.tm_hour = h;
  ti.tm_min  = m;
  ti.tm_sec  = 0;
  return mktime(&ti);
}

time_t end(const JSON& contest) {
  return begin(contest) + 60*time_t(contest("duration"));
}

time_t freeze(const JSON& contest) {
  return end(contest) - 60*int(contest("freeze"));
}

time_t blind(const JSON& contest) {
  return end(contest) - 60*int(contest("blind"));
}

bool allow_problem(const JSON& problem, int user) {
  int cid;
  if (!problem("contest").read(cid)) return true;
  DB(contests);
  JSON contest = contests.retrieve(cid);
  return
    contest("finished") ||
    isjudge(user,contest) ||
    time(contest, user).begin <= ::time(nullptr)
  ;
}

bool allow_create_attempt(JSON& attempt, const JSON& problem) {
  int cid;
  if (!problem("contest").read(cid)) return true;
  DB(contests);
  JSON contest = contests.retrieve(cid);
  if (contest("finished")) return true;
  if (isjudge(attempt["user"],contest)) {
    attempt["contest"] = cid;
    attempt["privileged"].settrue();
    return true;
  }
  auto t = time(contest,attempt["user"]);
  time_t when = attempt["when"];
  if (t.begin <= when && when < t.end) {
    attempt["contest"] = cid;
    attempt["contest_time"] = int(roundl((when-t.begin)/60.0L));
    return true;
  }
  return false;
}

JSON get(int id, int user) {
  DB(contests);
  JSON ans;

  if(!contests.retrieve(id,ans)) return JSON::null();

  Time tmp = time(ans, user);

  if (
    (!isjudge(user,ans) && ::time(nullptr) < tmp.begin) ||
    (!isjudge(user,ans) && ::time(nullptr) >= tmp.end)
  ) {
    return JSON::null();
  }

  ans["id"] = id;
  return ans;
}

JSON get_problems(int id, int user) {
  JSON contest = get(id,user);
  if (!contest) return contest;
  return list_problems(contest,user);
}

JSON get_attempts(int id, int user) {
  JSON contest = get(id,user);
  if (!contest) return contest;
  // get problem info
  JSON probs = list_problems(contest,user);
  map<int,JSON> pinfo;
  int i = 0;
  for (auto& prob : probs.arr()) pinfo[prob["id"]] = map<string,JSON>{
    {"id"   , prob["id"]},
    {"name" , prob["name"]},
    {"color", prob["color"]},
    {"idx"  , i++}
  };
  // get attempts
  JSON ans = Attempt::page(user,0,0,id);
  // set problem info
  for (auto& att : ans.arr()) att["problem"] = pinfo[att["problem"]["id"]];
  // no blind filtering needed?
  if (
    contest("finished") ||
    int(contest["blind"]) == 0 ||
    isjudge(user,contest)
  ) return ans;
  // blind filtering
  int blind = int(contest["duration"])-int(contest["blind"]);
  for (auto& att : ans.arr()) {
    if (int(att["contest_time"]) < blind) continue;
    att["status"] = "blind";
    att.erase("verdict");
  }
  return ans;
}

JSON page(int user, unsigned p, unsigned ps) {
  if(!user) return JSON(vector<JSON>{});
  string turma = User::get(user)["turma"];

  DB(contests);
  JSON ans(vector<JSON>{});
  contests.retrieve_page(p,ps,[&](const Database::Document& contest) {
    JSON tmp = contest.second;
    if (!tmp["start"].obj().count(turma)) return Database::null();
    tmp["id"] = contest.first;
    JSON tmp2 = tmp["start"][turma];
    tmp["start"] = tmp2;
    if(::time(nullptr) < end(tmp)) ans.push_back(move(tmp));
    return Database::null();
  });

  return ans;
}

JSON notas(){
  DB(attempts);
  DB(contests);
  DB(users);
  JSON atts = attempts.retrieve();
  JSON conts = contests.retrieve();
  JSON user = users.retrieve();

  JSON user_problem;
  for(JSON att : atts.arr()){
    string prob = att("problem");
    string user = att("user");
    if(!user_problem(user)) user_problem[user] = JSON();

    if(!user_problem(user, prob)){
      user_problem[user][prob] = JSON();
      user_problem[user][prob]["num"] = 0LL;
      user_problem[user][prob]["den"] = 1LL;
    }
    att.erase("ip");
    long long tmpnum = 0, tmpden = 1;
    if(att("verdict") == AC) tmpnum = tmpden = 1;
    else if(att("solved_tests")){
      tmpnum = att("solved_tests");
      tmpden = att("total_tests");
    }
    long long num = user_problem(user, prob, "num");
    long long den = user_problem(user, prob, "den");
    if(tmpnum * den > num * tmpden){ // tmpnum / tmpden > num / den
      user_problem[user][prob]["num"] = tmpnum;
      user_problem[user][prob]["den"] = tmpden;
    }
  }

  JSON ans = JSON(), ansprovas = JSON();

  for(JSON &us : user.arr()){
    int uid = us("id");
    string username = us("username");

    string uids = tostr(uid);
    string turma = us("turma");
    if(!ans(turma)) ans[turma] = JSON();
    if(!ans(turma, username)) ans[turma][username] = JSON();
    if(!ansprovas(turma)) ansprovas[turma] = JSON();
    if(!ansprovas(turma, username)) ansprovas[turma][username] = JSON();
    
    if(!user_problem(uids)) user_problem[uids] = JSON();
    for(JSON contest : conts.arr()){
      JSON probs;
      bool is_prova = false;
      if(contest("qnt_provas")){
        is_prova = true;
        int prova = (uid % int(contest("qnt_provas"))) + 1;
        probs = contest("prova", tostr(prova));
      }
      else probs = contest("problems");

      long long num = 0, den = 1, sz = 0;

      for(int pid : probs.arr()){
        string pids = tostr(pid);
        if(user_problem(uids, pids)){
          long long tmpnum = user_problem(uids, pids, "num");
          long long tmpden = user_problem(uids, pids, "den");
          num = num * tmpden + tmpden * den;
          den = den * tmpden;
          long long g = gcd(num, den);
          if(g) num /= g, den /= g;
        }
      }
      den *= (long long)probs.arr().size();
      long long g = gcd(num, den);
      if(g) num /= g, den /= g;
      string cid = contest("id");
      if(!is_prova){
        if(!ans(turma, username, cid)) ans[turma][username][cid] = JSON();

        ans[turma][username][cid] = JSON();
        ans[turma][username][cid]["name"] = contest("name");
        ans[turma][username][cid]["num"] = num;
        ans[turma][username][cid]["den"] = den;
      }
      else{
        if(!ansprovas(turma, username, cid)) ansprovas[turma][username][cid] = JSON();

        ansprovas[turma][username][cid] = JSON();
        ansprovas[turma][username][cid]["name"] = contest("name");
        ansprovas[turma][username][cid]["num"] = num;
        ansprovas[turma][username][cid]["den"] = den;        
      }
    }
  }

  return JSON(vector<JSON>{ans, ansprovas});
}

} // namespace Contest
