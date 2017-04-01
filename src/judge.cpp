#include <queue>

#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <fstream>
#include <algorithm>
#include "judge.hpp"

#include "database.hpp"
#include "helper.hpp"
#include "language.hpp"

using namespace std;

// %p = path
// %s = source
// %P = problem
static string command(
  const string& srccmd,
  const string& path,
  const string& prob,
  const string& lang
) {
  string cmd;
  for (const char* s = srccmd.c_str(); *s; s++) {
    if (*s != '%') { cmd += *s; continue; }
    s++;
    switch (*s) {
      case 'p': cmd += path;                break;
      case 's': cmd += path+"/"+prob+lang;  break;
      case 'P': cmd += prob;                break;
      default:  cmd += *(--s);              break;
    }
  }
  return cmd;
}

static char run(const string& cmd, int tls, int mlkB, int& mtms, int& mmkB) {
  // init time
  timeval start;
  gettimeofday(&start,nullptr);
  // child
  pid_t pid = fork();
  if (!pid) {
    tls++;
    rlimit r;
    r.rlim_cur = tls;
    r.rlim_max = tls;
    setrlimit(RLIMIT_CPU,&r);
    r.rlim_cur = RLIM_INFINITY;
    r.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_STACK,&r);
    execl("/bin/sh","/bin/sh","-c",cmd.c_str(),(char*)0);
    _exit(-1);
  }
  // parent
  int st;
  rusage r;
  wait4(pid,&st,0,&r);
  long long us = 0;
  us += r.ru_utime.tv_sec*1000000L;
  us += r.ru_utime.tv_usec;
  us += r.ru_stime.tv_sec*1000000L;
  us += r.ru_stime.tv_usec;
  mtms = us/1000;
  mmkB = r.ru_maxrss;
  if (mtms > tls*1000) return TLE;
  if (mmkB > mlkB) return MLE;
  if (!WIFEXITED(st) || WEXITSTATUS(st) || WIFSIGNALED(st)) return RTE;
  return AC;
}

bool output_is_correct(const string &outputfile, const string &sourcefile){
  string output;
  ifstream outputstream(outputfile);
  ifstream sourcestream(sourcefile);

  if(!outputstream.is_open()) return false;
  if(!sourcestream.is_open()) return false;

  string word;
  while(outputstream >> word){

    auto it = copy_if(word.begin(), word.end(), word.begin(), [](char c){
      switch(c){
        case '\t': case '\r': case '\n': case  '.':
        case  ',': case  ':': case  ';': case  ' ':
          return false;
      }
      return true;
    });

    word.resize(distance(word.begin(), it));

    transform(word.begin(), word.end(), back_inserter(output), ::tolower);
  }

  outputstream.close();

  int last = 0;
  vector<int> preffix_function;
  bool answer = true;
  while(sourcestream >> word){

    auto it = copy_if(word.begin(), word.end(), word.begin(), [](char c){
      switch(c){
        case '\t': case '\r': case '\n': case  '.':
        case  ',': case  ':': case  ';': case  ' ':
          return false;
      }
      return true;
    });

    word.resize(distance(word.begin(), it));

    transform(word.begin(), word.end(), word.begin(), ::tolower);

    preffix_function.assign(word.size()+1, 0);

    for(int i = 2; i <= word.size(); i++){
      int j = preffix_function[i-1];
      while(j > 0 && word[j] != word[i-1])
        j = preffix_function[j];
      if(word[j] == word[i-1]) j++;
      preffix_function[i] = j;
    }

    int state = 0;

    bool reach_final_state = false;

    while(last < output.size()){
      while(state > 0 && output[last] != word[state])
        state = preffix_function[state];
      if(output[last] == word[state])
        state++;

      last++;
      if(state == word.size()){
        reach_final_state = true;
        break;
      }
    }

    if(!reach_final_state){
      answer = false;
      break;
    }
  }
  sourcestream.close();
  return answer;
}

static void judge(int attid) {
  // load stuff
  DB(attempts);
  JSON att;
  if (!attempts.retrieve(attid,att)) return;
  string prob = att["problem"];
  string lang = att["language"];
  JSON settings = move(Language::settings(att));
  if (!settings) { // impossible to judge
    att["status"] = "cantjudge";
    attempts.update(attid,move(att));
    return;
  }
  
  string path = "attempts/"+tostr(attid);
  
  // compile
  if (settings("compile")) {
    int status = system(command(settings["compile"],path,prob,lang).c_str());
    if (WEXITSTATUS(status)) {
      att["status"] = "judged"; // CE needs no judgement by humans
      att["verdict"] = verdict_tos(CE);
      attempts.update(attid,move(att));
      return;
    }
  }
  
  // get execution settings
  string cmd = command(settings["run"],path,prob,lang);
  int Mtms=0,MmkB=0,mtms,mmkB;
  int tls = settings["timelimit"], mlkB = settings["memlimit"];
  
  // init attempt
  att["status"] = (
    att("privileged") || settings("autojudge") ? "judged": "waiting"
  );
  int verd = AC;
  
  // for each input file
  string dn = "problems/"+prob;
  DIR* dir = opendir((dn+"/input").c_str());
  for (dirent* ent = readdir(dir); ent; ent = readdir(dir)) {
    string fn = ent->d_name;
    string ifn = dn+"/input/"+fn;
    
    // check if dirent is regular file
    struct stat stt;
    stat(ifn.c_str(),&stt);
    if (!S_ISREG(stt.st_mode)) continue;
    
    // run
    string ofn = path+"/output/"+fn;
    verd = run(cmd+" < "+ifn+" > "+ofn,tls,mlkB,mtms,mmkB);
    Mtms = max(Mtms,mtms);
    MmkB = max(MmkB,mmkB);
    if (verd != AC) break;
    
    // diff
    string sfn = dn+"/output/"+fn;
    if(!output_is_correct(ofn, sfn)){ verd = WA; break; }
    
    // remove correct output
    remove(ofn.c_str());
  }
  closedir(dir);
  
  // update attempt
  att["verdict"] = verdict_tos(verd);
  att["time"] = move(tostr(mtms));
  att["memory"] = move(tostr(mmkB));
  attempts.update(attid,move(att));
}

static queue<int> jqueue;
static bool quit = false;
static pthread_t jthread;
static pthread_mutex_t judge_mutex = PTHREAD_MUTEX_INITIALIZER;
static void* thread(void*) {
  while (!quit) {
    pthread_mutex_lock(&judge_mutex);
    if (jqueue.empty()) {
      pthread_mutex_unlock(&judge_mutex);
      usleep(25000);
      continue;
    }
    int attid = jqueue.front(); jqueue.pop();
    pthread_mutex_unlock(&judge_mutex);
    judge(attid);
  }
}

namespace Judge {

void init() {
  DB(attempts);
  JSON tmp = attempts.retrieve(JSON(map<string,JSON>{
    {"status", "queued"}
  }));
  for (auto& a : tmp.arr()) jqueue.push(a["id"]);
  pthread_create(&jthread,nullptr,thread,nullptr);
}

void close() {
  quit = true;
  pthread_join(jthread,nullptr);
}

void push(int attid) {
  DB(attempts);
  if (attempts.update([](Database::Document& doc) {
    if (doc.second["status"] == "queued") return false;
    doc.second["status"] = "queued";
    return true;
  },attid)) {
    pthread_mutex_lock(&judge_mutex);
    jqueue.push(attid);
    pthread_mutex_unlock(&judge_mutex);
  }
}

void push_problem(int probid) {
  DB(attempts);
  vector<int> atts;
  attempts.retrieve([&](const Database::Document& doc) {
    if (doc.second("problem") == probid) atts.push_back(doc.first);
    return Database::null();
  });
  for (int id : atts) push(id);
}

} // namespace Judge
