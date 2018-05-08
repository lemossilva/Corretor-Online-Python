#ifndef CONTEST_H
#define CONTEST_H

#include "database.hpp"

namespace Contest {

void fix();

struct Time {
  time_t begin,end,freeze,blind;
};
Time time(const JSON& contest, int user);
time_t begin(const JSON& contest);
time_t end(const JSON& contest);
time_t freeze(const JSON& contest);
time_t blind(const JSON& contest);

bool allow_problem(const JSON& problem, int user);
bool allow_create_attempt(JSON& attempt, const JSON& problem);

bool is_user_allowed(int cid, int user);
void disallow_user(int cid, int user, int tutor);
void allow_user(int cid, int user, int tutor);

JSON get_allowed_turma(int cid, int user, const std::string &turma);
JSON get(int id, int user);
JSON get_problems(int id, int user);
JSON get_attempts(int id, int user);
JSON scoreboard(int id, int user);
JSON page(int user = 0, unsigned page = 0, unsigned page_size = 0);
JSON notas();
JSON get_all(int user = 0);
JSON get_provas(int user);
} // namespace Contest

#endif
