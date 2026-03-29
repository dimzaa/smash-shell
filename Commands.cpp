#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <sys/types.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include <pwd.h>
#include <grp.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <fstream>
using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()\
cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()\
cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif
struct linux_b {
  ino64_t        d_ino;      // inode number
  off64_t        d_off;      // offset to next dirent
  unsigned short d_reclen;   // length of this record
  unsigned char  d_type;     // file type
  char           d_name[];   // filename (null-terminated)
};
string _ltrim(const std::string & s) {
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string & s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string & s) {
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char * cmd_line, char ** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for (std::string s; iss >> s;) {
    args[i] = (char * ) malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char * cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char * cmd_line) {
  const string str(cmd_line);
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  if (idx == string::npos) {
    return;
  }
  if (cmd_line[idx] != '&') {
    return;
  }
  cmd_line[idx] = ' ';
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

SmallShell::SmallShell(): prompt("smash"), smash_pid(getpid()), fore_pid(-1), lastPwd(nullptr) {

}
SmallShell::~SmallShell() {
  if (lastPwd != nullptr) {
    free(lastPwd);
  }
}
void SmallShell::setPrompt(const std::string & new_prompt) {
  prompt = new_prompt.empty() ? "smash" : new_prompt;
}

const string & SmallShell::getPrompt() const {
  return prompt;
}

pid_t SmallShell::getSmashPid() const {
  return smash_pid;
}

void SmallShell::setForegroundPID(pid_t pid) {
  fore_pid = pid;
}

pid_t SmallShell::getForePID() const {
  return fore_pid;
}

char* SmallShell::getLastPwd() const {
  return lastPwd;
}

void SmallShell::setLastPwd(const char* newPwd) {
  if (lastPwd != nullptr) {
    free(lastPwd);
  }
  lastPwd = newPwd ? strdup(newPwd) : nullptr;
}
static Command* b_command(const std::string& target) {
  std::istringstream stream(target);
  std::string cmd;
  stream >> cmd;

  SmallShell& smash = SmallShell::getInstance();
  smash.jobsList.removeFinishedJobs();
  const char* dup = strdup(target.c_str());
  if (cmd == "chprompt") return new ChpromptCommand(dup);
  if (cmd == "showpid")  return new ShowPidCommand(dup);
  if (cmd == "cd")       return new ChangeDirCommand(dup, smash.getLastPwd());
  if (cmd == "pwd")      return new GetCurrDirCommand(dup);
  if (cmd == "jobs")     return new JobsCommand(dup, &smash.jobsList);
  if (cmd == "kill")     return new KillCommand(dup, &smash.jobsList);
  if (cmd == "fg")       return new ForegroundCommand(dup, &smash.jobsList);
  if (cmd == "quit")     return new QuitCommand(dup, &smash.jobsList);
  if (cmd == "alias")    return new AliasCommand(dup);
  if (cmd == "unalias")  return new UnAliasCommand(dup);
  if (cmd == "sysinfo")  return new SysInfoCommand(dup);
  if (cmd == "du")       return new DiskUsageCommand(dup);
  if (cmd == "whoami")   return new WhoAmICommand(dup);
  if (cmd == "usbinfo")  return new USBInfoCommand(dup);
  if (cmd == "unsetenv") return new UnSetEnvCommand(dup);

  return nullptr;
}

Command* SmallShell::CreateCommand(const char* cmd_line) {
  std::string cleaned = _trim(std::string(cmd_line));
  {
    std::istringstream stream(cleaned);
    std::string first;
    stream >> first;
    std::string expanded = AliasManager::getInstance().getAlias(first);
    if (!expanded.empty()) {
      size_t pos = cleaned.find(first);
      cleaned.replace(pos, first.length(), expanded);
      if (_isBackgroundComamnd(cmd_line) && !_isBackgroundComamnd(cleaned.c_str()))
        cleaned += "&";
    }
  }
  if (cleaned.find('>') != std::string::npos)
    return new RedirectionCommand(cleaned.c_str());

  if (cleaned.find('|') != std::string::npos &&
      cleaned.find("alias") == std::string::npos)
    return new PipeCommand(cleaned.c_str());

  Command* builtin = b_command(cleaned);
  if (builtin) {
    return builtin;
  }
  return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char* cmd_line) {
  Command* command = CreateCommand(cmd_line);
  if (!command) {
    return;
  }
  SmallShell& shell = SmallShell::getInstance();
  shell.setForegroundPID(command->getPid());
  command->execute();
  shell.setForegroundPID(-1);
  delete command;
}
void ChpromptCommand::execute() {
  char * args[COMMAND_MAX_ARGS];
  int arCount = _parseCommandLine(cmd_line, args);

  SmallShell & smash = SmallShell::getInstance();

  if (arCount > 1) {
    smash.setPrompt(args[1]);
  } else {
    smash.setPrompt("");
  }
}
void ShowPidCommand::execute() {
  SmallShell & curr_smash = SmallShell::getInstance();
  std::cout << "smash pid is " << curr_smash.getSmashPid() << std::endl;;
}

void GetCurrDirCommand::execute() {
  char curr_wd[PATH_MAX];
  if (getcwd(curr_wd, sizeof(curr_wd)) != nullptr) {
    std::cout << curr_wd << std::endl;
  } else {
    perror("smash error: getcwd failed");
  }
}
const char* ChangeDirCommand::getTargetPath(char* args[], int argc, const char* lastPwd) {
  if (argc == 1)
    return nullptr;

  const char* path = args[1];
  if (strcmp(path, "-") == 0) {
    if (!lastPwd) {
      std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
      return nullptr;
    }
    return lastPwd;
  }
  return path;
}
void ChangeDirCommand::execute() {
  char* args[COMMAND_MAX_ARGS];
  int argc = _parseCommandLine(cmd_line, args);

  if (argc > 2) {
    std::cerr << "smash error: cd: too many arguments" << std::endl;
    for (int i = 0; i < argc; ++i) free(args[i]);
    return;
  }
  char prevCwd[PATH_MAX];
  if (!getcwd(prevCwd, sizeof(prevCwd))) {
    perror("smash error: getcwd failed");
    for (int i = 0; i < argc; ++i) free(args[i]);
    return;
  }
  const char* target = getTargetPath(args, argc, lastPwd);

  if (argc != 1) {
    if (!target) {
      for (int i = 0; i < argc; ++i) free(args[i]);
      return;
    }
    if (chdir(target) != 0) {
      perror("smash error: chdir failed");
      for (int i = 0; i < argc; ++i) free(args[i]);
      return;
    }
  }
  SmallShell::getInstance().setLastPwd(prevCwd);

  for (int i = 0; i < argc; ++i){ free(args[i]);}
}

void ExternalCommand::executeChild(const std::string& cmd_s) {
  setpgrp();
  bool complex = (strpbrk(cmd_s.c_str(), "*?") != nullptr);
  if (!complex) {
    char* args[COMMAND_MAX_ARGS];
    int argc = _parseCommandLine(cmd_s.c_str(), args);
    args[argc] = nullptr;

    execvp(args[0], args);
    perror("smash error: execvp failed");
    exit(EXIT_FAILURE);
  }
  char* args[] = {
    const_cast<char*>("/bin/bash"),
    const_cast<char*>("-c"),
    strdup(cmd_s.c_str()),
    nullptr
};
  execv("/bin/bash", args);
  perror("smash error: execv failed");
  exit(EXIT_FAILURE);
}

void ExternalCommand::execute() {
  bool isBackground = _isBackgroundComamnd(cmd);
  if (isBackground) {
    _removeBackgroundSign(cmd);
  }

  AliasManager& aliasManager = AliasManager::getInstance();
  std::string cmd_s = _trim(std::string(cmd));

  std::istringstream iss(cmd_s);
  std::string token;
  std::vector<std::string> tokens;

  while (iss >> token)
    tokens.push_back(token);
  if (!tokens.empty()) {
    std::string firstToken = tokens[0];
    std::string expanded = aliasManager.getAlias(firstToken);
    if (!expanded.empty()) {
      tokens[0] = expanded;
      cmd_s.clear();
      for (const std::string& t : tokens)
        cmd_s += t + " ";
      cmd_s = _trim(cmd_s);
    }
  }
  pid_t pid = fork();
  if (pid == -1) {
    perror("smash error: fork failed");
    return;
  }
  if (pid == 0) {
    executeChild(cmd_s);
    return;
  }

  this->pid = pid;
  SmallShell& smash = SmallShell::getInstance();

  if (isBackground) {
    smash.jobsList.addJob(this, false);
  } else {
    smash.setForegroundPID(pid);
    int status;
    waitpid(pid, &status, WUNTRACED);
    smash.setForegroundPID(-1);
  }
}

void JobsList::addJob(Command* cmd, bool isStopped) {
  removeFinishedJobs();
  int nextId = 1;
  if (!jobs.empty()) {
    nextId = jobs.rbegin()->first + 1;
  }
  JobEntry entry(cmd, nextId, isStopped, cmd->getPid());
  jobs.emplace(nextId, entry);
}
void JobsList::printJobsList() {
  removeFinishedJobs();
  for (const auto &job : jobs) {
    std::cout << "[" << job.second.jobId << "] " << job.second.cmdLine << (job.second.isStopped ? " (stopped)" : "") << std::endl;
  }
}

void JobsList::killAllJobs() {
  for (auto &job : jobs) {
    kill(job.second.pid, SIGKILL);
  }
  jobs.clear();
}

void JobsList::removeFinishedJobs() {
  auto index = jobs.begin();
  while (index != jobs.end()) {
    pid_t pid = index->second.pid;
    if (waitpid(pid, nullptr, WNOHANG) > 0) {
      index = jobs.erase(index);
    } else {
      ++index;
    }
  }
}


JobsList::JobEntry* JobsList::getJobById(int jobId) {
  auto it = jobs.find(jobId);
  return (it == jobs.end()) ? nullptr : &it->second;
}





JobsList::JobEntry* ForegroundCommand::getJobForFg(const std::vector<std::string>& tokens, int& jobId) {
  if (tokens.size() == 1) {
    int lastId;
    JobsList::JobEntry* job = jobs->getLastJob(&lastId);

    if (!job) {
      std::cerr << "smash error: fg: jobs list is empty" << std::endl;
      return nullptr;
    }
    jobId = lastId;
    return job;
  }
  if (tokens.size() == 2) {
    try {
      jobId = std::stoi(tokens[1]);
    } catch (...) {
      std::cerr << "smash error: fg: invalid arguments" << std::endl;
      return nullptr;
    }
    if (jobId <= 0) {
      std::cerr << "smash error: fg: invalid arguments" << std::endl;
      return nullptr;
    }
    JobsList::JobEntry* job = jobs->getJobById(jobId);
    if (!job) {
      std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
      return nullptr;
    }
    return job;
  }
  std::cerr << "smash error: fg: invalid arguments" << std::endl;
  return nullptr;
}
void JobsList::removeJobById(int jobId) {
  jobs.erase(jobId);
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
  if (jobs.empty()) {
    return nullptr;
  }
  *lastJobId = jobs.rbegin()->first;
  return &jobs.rbegin()->second;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
  for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
    if (it->second.isStopped) {
      *jobId = it->first;
      return &it->second;
    }
  }
  return nullptr;
}
void ForegroundCommand::execute() {
  std::istringstream file(getCmdLine());
  std::vector<std::string> tokens;
  std::string tok;
  while (file >> tok) {
    tokens.push_back(tok);
  }
  int jobId;
  JobsList::JobEntry* job = getJobForFg(tokens, jobId);
  if (!job) {
    return;
  }
  std::cout << job->cmdLine << " " << job->pid << std::endl;
  jobs->removeJobById(jobId);
  SmallShell::getInstance().setForegroundPID(job->pid);
  int status;
  waitpid(job->pid, &status, WUNTRACED);
  SmallShell::getInstance().setForegroundPID(-1);
}
void JobsCommand::execute() {
  jobs->printJobsList();
}
void QuitCommand::killAllJobsAndPrint() {
  int count = jobs->jobs.size();

  std::cout << "smash: sending SIGKILL signal to " << count << " jobs:" << std::endl;

  for (const auto& i : jobs->jobs) {
    const auto& job = i.second;

    std::cout << job.pid << ": " << job.cmdLine << std::endl;
    kill(job.pid, SIGKILL);
  }
}

void QuitCommand::execute() {
  std::istringstream file(getCmdLine());
  std::vector<std::string> tokens;
  std::string tok;
  while (file >> tok)
    tokens.push_back(tok);

  if (tokens.size() >= 2 && tokens[1] == "kill") {
    killAllJobsAndPrint();
  }
  exit(0);
}

bool KillCommand::parseKillArgs(const std::vector<std::string>& args,int& signalNum, int& jobId){
  if (args.size() != 3) {
    std::cerr << "smash error: kill: invalid arguments" << std::endl;
    return false;
  }

  const std::string& sigStr = args[1];
  const std::string& idStr  = args[2];

  if (sigStr.empty() || sigStr[0] != '-') {
    std::cerr << "smash error: kill: invalid arguments" << std::endl;
    return false;
  }
  try {
    signalNum = std::stoi(sigStr.substr(1));
  } catch (...) {
    std::cerr << "smash error: kill: invalid arguments" << std::endl;
    return false;
  }
  try {
    jobId = std::stoi(idStr);
    if (jobId <= 0)
      throw std::invalid_argument("bad");
  } catch (...) {
    std::cerr << "smash error: kill: invalid arguments" << std::endl;
    return false;
  }

  return true;
}
void KillCommand::execute() {
  std::istringstream file(getCmdLine());
  std::vector<std::string> tokens;
  std::string word;
  while (file >> word) {
    tokens.push_back(word);
  }  int signalNum, jobId;
  if (!parseKillArgs(tokens, signalNum, jobId))
    return;

  JobsList::JobEntry* targetJob = jobs->getJobById(jobId);
  if (!targetJob) {
    std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
    return;
  }
  if (kill(targetJob->pid, signalNum) == -1) {
    std::cout << "signal number " << signalNum << " was sent to pid " << targetJob->pid << std::endl;
    perror("smash error: kill failed");
    return;
  }
  std::cout << "signal number " << signalNum << " was sent to pid " << targetJob->pid << std::endl;
}
bool AliasManager::insertAlias(const std::string& name, const std::string& command) {
  bool exists = aliases.find(name) != aliases.end();
  if (if_Res(name) || exists) {
    std::cerr << "smash error: alias: " << name
              << " already exists or is a reserved command" << std::endl;
    return false;
  }
  al_List.emplace_back(name, command);
  aliases[name] = std::prev(al_List.end());
  return true;
}
bool AliasManager::remAlias(const std::string& name) {
  auto it = aliases.find(name);
  if (it != aliases.end()) {
    al_List.erase(it->second);
    aliases.erase(it);
    return true;
  }
  return false;
}
std::string AliasManager::getAlias(const std::string& name) const {
  auto it = aliases.find(name);
  if (it == aliases.end())
    return "";
  return it->second->second;
}
void AliasManager::printAliases() const {
  for (const auto& alias : al_List) {
    std::cout << alias.first << "='" << alias.second << "'" << std::endl;
  }
}

bool AliasManager::if_Res(const std::string& name) const {
  return taken_words.find(name) != taken_words.end();
}
bool AliasCommand::parseAliasCommand(const std::string& cmdStr,std::string& name,std::string& command)
{
  std::smatch match;
  std::regex aliasRegex(R"(^alias ([a-zA-Z0-9_]+)='([^']*)'$)");
  if (!std::regex_match(cmdStr, match, aliasRegex)) {
    std::cerr << "smash error: alias: invalid alias format" << std::endl;
    return false;
  }
  name = match[1];
  command = match[2];
  return true;
}

void AliasCommand::execute() {
  AliasManager& aliasManager = AliasManager::getInstance();
  char* tmp = strdup(cmd_line);
  if (_isBackgroundComamnd(tmp))
    _removeBackgroundSign(tmp);
  std::string cmdStr = _trim(tmp);
  free(tmp);

  std::istringstream file(cmdStr);
  std::string firstWord;
  file >> firstWord;
  if (firstWord == "alias" && file.eof()) {
    aliasManager.printAliases();
    return;
  }
  std::string name, command;
  if (!parseAliasCommand(cmdStr, name, command))
    return;

  aliasManager.insertAlias(name, command);
}

int RedirectionCommand::openRedirectFile() const {
  mode_t old_umask = umask(0);
  int flags = app ?
      (O_WRONLY | O_CREAT | O_APPEND) :
      (O_WRONLY | O_CREAT | O_TRUNC);
  int fd = open(file.c_str(), flags, 0666);
  umask(old_umask);
  if (fd == -1) {
    perror("smash error: open failed");
  }
  return fd;
}

RedirectionCommand::RedirectionCommand(const char* cmd_line): Command(cmd_line), app(false){
  char* temp = strdup(cmd_line);
  std::string line(temp);
  free(temp);
  if (_isBackgroundComamnd(line.c_str())) {
    temp = strdup(line.c_str());
    _removeBackgroundSign(temp);
    line = temp;
    free(temp);
  }
  size_t Pos = line.find('>');
  if (Pos == std::string::npos) {
    return;
  }
  cmd = _trim(line.substr(0, Pos));
  if (Pos + 1 < line.size() && line[Pos + 1] == '>') {
    app = true;
    Pos++;
  }
  file = _trim(line.substr(Pos + 1));
}
void RedirectionCommand::execute() {
  int saved_stdout = dup(STDOUT_FILENO);
  if (saved_stdout == -1) {
    perror("dup failed");
    return;
  }
  int fd = openRedirectFile();
  if (fd == -1) {
    close(saved_stdout);
    return;
  }
  if (dup2(fd, STDOUT_FILENO) == -1) {
    perror("dup2 failed");
    close(fd);
    close(saved_stdout);
    return;
  }
  close(fd);
  Command* inner = SmallShell::getInstance().CreateCommand(cmd.c_str());
  if (inner) {
    inner->execute();
    delete inner;
  }
  if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
    perror("dup2 restore failed");
  }

  close(saved_stdout);
}

void UnAliasCommand::execute() {
  AliasManager& aliasManager = AliasManager::getInstance();
  char* cmd_tmp = strdup(cmd_line);
  if (_isBackgroundComamnd(cmd_tmp)) _removeBackgroundSign(cmd_tmp);
  std::string cmdStr(cmd_tmp);
  free(cmd_tmp);
  cmdStr = _trim(cmdStr);

  std::istringstream file(cmdStr);
  std::string command;
  file >> command;

  std::vector<std::string> names;
  std::string name;
  while (file >> name) {
    names.push_back(name);
  }

  if (names.empty()) {
    std::cerr << "smash error: unalias: not enough arguments" << std::endl;
    return;
  }
  for (const auto& aliasName : names) {
    if (!aliasManager.remAlias(aliasName)) {
      std::cerr << "smash error: unalias: " << aliasName << " alias does not exist" << std::endl;
      return;
    }
  }
}




PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line), stderr_pipe(false) {
  std::string cmd_str(cmd_line);
  size_t pos = cmd_str.find("|&");
  if (pos != std::string::npos) {
    stderr_pipe = true;
    cmd1 = _trim(cmd_str.substr(0, pos));
    cmd2 = _trim(cmd_str.substr(pos + 2));
  } else {
    pos = cmd_str.find('|');
    if (pos != std::string::npos) {
      cmd1 = _trim(cmd_str.substr(0, pos));
      cmd2 = _trim(cmd_str.substr(pos + 1));
    }
  }
}

void PipeCommand::execute() {
  int pfd[2];
  if (pipe(pfd) < 0) {
    perror("pipe failed");
    return;
  }
  pid_t left = fork();
  if (left < 0) {
    perror("fork failed");
    return;
  }
  if (left == 0) {
    setpgrp();
    int out_fd = stderr_pipe ? STDERR_FILENO : STDOUT_FILENO;
    if (dup2(pfd[1], out_fd) < 0) {
      perror("dup2 failed");
      exit(1);
    }
    close(pfd[0]);
    close(pfd[1]);
    Command* left_cmd = SmallShell::getInstance().CreateCommand(cmd1.c_str());
    if (left_cmd) {
      left_cmd->execute();
      delete left_cmd;
    }
    exit(0);
  }
  pid_t right = fork();
  if (right < 0) {
    perror("fork failed");
    return;
  }
  if (right == 0) {
    setpgrp();
    if (dup2(pfd[0], STDIN_FILENO) < 0) {
      perror("dup2 failed");
      exit(1);
    }
    close(pfd[0]);
    close(pfd[1]);
    Command* right_cmd = SmallShell::getInstance().CreateCommand(cmd2.c_str());
    if (right_cmd) {
      right_cmd->execute();
      delete right_cmd;
    }
    exit(0);
  }
  close(pfd[0]);
  close(pfd[1]);
  waitpid(left, nullptr, 0);
  waitpid(right, nullptr, 0);
}

std::string fTime(long ts) {
  time_t t = (time_t)ts;
  struct tm* tm_info = localtime(&t);

  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

  return std::string(buffer);
}

long readBTime() {
  std::ifstream file("/proc/stat");
  std::string curr_key;
  long btime = 0;

  while (file >> curr_key) {
    if (curr_key == "btime") {
      file >> btime;
      break;
    }
  }
  return btime;
}
std::string readFirstL(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return "";
  }
  std::string line;
  std::getline(file, line);
  return line;
}
void SysInfoCommand::execute() {
  std::string hostname = readFirstL("/proc/sys/kernel/hostname");
  std::string kernel = readFirstL("/proc/sys/kernel/osrelease");
  std::string system = readFirstL("/proc/sys/kernel/ostype");
  std::string arch = "x86_64";

  long boot_ts = readBTime();
  std::string boot_time_str = fTime(boot_ts);

  std::cout << "System: " << system << std::endl;
  std::cout << "Hostname: " << hostname << std::endl;
  std::cout << "Kernel: " << kernel << std::endl;
  std::cout << "Architecture: " << arch << std::endl;
  std::cout << "Boot Time: " << boot_time_str << std::endl;
}
size_t calcuse(const std::string& path) {
  struct stat st;
  lstat(path.c_str(), &st);
  size_t total = st.st_blocks * 512;

  if (!S_ISDIR(st.st_mode)) {
    return total;
  }
  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0) return total;
  char buffer[4096];
  while (true) {
    int nread = syscall(SYS_getdents64, fd, buffer, sizeof(buffer));
    if (nread <= 0) break;

    for (int i = 0; i < nread; ) {
      auto* d = (struct linux_b*)(buffer + i);
      std::string name = d->d_name;

      if (name != "." && name != "..") {
        std::string next = path + "/" + name;
        total += calcuse(next);
      }

      i += d->d_reclen;
    }
  }

  close(fd);
  return total;
}


void DiskUsageCommand::execute() {
  char* args[COMMAND_MAX_ARGS];
  int argc = _parseCommandLine(cmd_line, args);

  if (argc > 2) {
    std::cerr << "smash error: du: too many arguments" << std::endl;
    return;
  }

  std::string path_curr;
  if (argc == 1) {
    char cwd[4096];
    getcwd(cwd, sizeof(cwd));
    path_curr = cwd;
  } else {
    path_curr = args[1];
  }
  size_t total_bytes = calcuse(path_curr);

  size_t total_kb = (total_bytes + 1023) / 1024;
  std::cout << "Total disk usage: " << total_kb << " KB" << std::endl;
}
static void showUser(const passwd* pw, uid_t uid_val, gid_t gid_val) {
  std::cout << pw->pw_name << std::endl;
  std::cout << uid_val << std::endl;
  std::cout << gid_val << std::endl;
  std::cout << pw->pw_dir << std::endl;
}
void WhoAmICommand::execute() {
  std::istringstream ss(getCmdLine());
  std::string word;
  std::vector<std::string> args;
  while (ss >> word) {
    args.push_back(word);
  }
  uid_t uid_val = getuid();
  gid_t gid_val = getgid();
  passwd* info = getpwuid(uid_val);
  if (!info) {
    perror("smash error: getpwuid failed");
    return;
  }
  showUser(info, uid_val, gid_val);
}

static std::string read_first_line_sys(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
      return "N/A";
    }
    char buffer[256];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (n <= 0) {
      return "N/A";
    }
    buffer[n] = '\0';
    std::string s(buffer);

    size_t pos = s.find('\n');
    if (pos != std::string::npos)
        s = s.substr(0, pos);
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.pop_back();
    if (s.empty()) {
      return "N/A";
    }
    return s;
}

static bool file_exists_sys(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    close(fd);
    return true;
}


static std::vector<int> splitnums(const std::string& s) {
  std::vector<int> out;
  std::string now;
  for (char c : s) {
    if (c == '-' || c == '.') {
      out.push_back(std::atoi(now.c_str()));
      now.clear();
    } else {
      now += c;
    }
  }
  if (!now.empty()) {
    out.push_back(std::atoi(now.c_str()));
  }
  return out;
}
struct Dev {
  std::string dir;
  std::string vendor;
  std::string product;
  std::string manufacturer;
  std::string product_name;
  std::string max_power;
  std::string devnum;
};
void printDeviceList(const std::vector<Dev>& list) {
  for (size_t i = 0; i < list.size(); i++) {
    const Dev& d = list[i];
    std::cout << "Device " << d.devnum << ": "
              << "ID " << d.vendor << ":" << d.product << " "
              << d.manufacturer << " " << d.product_name
              << " MaxPower: " << d.max_power << std::endl;
  }
}

void USBInfoCommand::execute() {
    const std::string base = "/sys/bus/usb/devices";
    int dirfd = open(base.c_str(), O_RDONLY);
    if (dirfd < 0) {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }
    std::vector<Dev> list;
    char buf[4096];

    while (true) {
        int nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf));
        if (nread <= 0)
            break;

        for (int i = 0; i < nread; ) {
            linux_b* d = (linux_b*)(buf + i);
            std::string name = d->d_name;
            i += d->d_reclen;

            if (name == "." || name == "..")
                continue;

            std::string devpath = base + "/" + name;
            std::string vfile = devpath + "/idVendor";
            std::string pfile = devpath + "/idProduct";

            if (!file_exists_sys(vfile) || !file_exists_sys(pfile))
                continue;

            Dev dv;
            dv.dir = name;
            dv.vendor = read_first_line_sys(vfile);
            dv.product = read_first_line_sys(pfile);
            dv.manufacturer = read_first_line_sys(devpath + "/manufacturer");
            dv.product_name = read_first_line_sys(devpath + "/product");
            dv.max_power = read_first_line_sys(devpath + "/bMaxPower");
            dv.devnum = read_first_line_sys(devpath + "/devnum");

            if (dv.max_power != "N/A") {
                while (!dv.max_power.empty() &&
                       (dv.max_power.back()==' ' || dv.max_power.back()=='\t' || dv.max_power.back()=='\r'))
                    dv.max_power.pop_back();

                if (dv.max_power.size() < 2 ||
                    dv.max_power.substr(dv.max_power.size()-2) != "mA")
                {
                    dv.max_power += "mA";
                }
            }

            if (dv.vendor == "1d6b" &&
               (dv.product == "0001" || dv.product == "0002" || dv.product == "0003"))
                continue;

            list.push_back(dv);
        }
    }

    close(dirfd);

    if (list.empty()) {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }
    std::sort(list.begin(), list.end(),
        [](const Dev& a, const Dev& b) {
            auto A = splitnums(a.dir);
            auto B = splitnums(b.dir);

            size_t n = std::min(A.size(), B.size());
            for (size_t i = 0; i < n; i++) {
                if (A[i] != B[i])
                    return A[i] < B[i];
            }
            return A.size() < B.size();
        }
    );
    printDeviceList(list);
}


extern char **environ;
bool eexists(const std::string& varname) {
  const std::string key = varname + "=";
  int fd = open("/proc/self/environ", O_RDONLY);
  if (fd < 0) {
    perror("smash error: open /proc/self/environ failed");
    return false;
  }
  char buf[8192];
  ssize_t sz = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (sz < 0) {
    perror("smash error: read /proc/self/environ failed");
    return false;
  }
  buf[sz] = '\0';
  size_t offset = 0;
  while (offset < static_cast<size_t>(sz)) {
    const char* entry = buf + offset;
    size_t entry_len = strlen(entry);
    if (entry_len >= key.size()) {
      if (strncmp(entry, key.c_str(), key.size()) == 0) {
        return true;
      }
    }
    offset += entry_len + 1;
  }
  return false;
}


bool remove_var(const std::string& varname) {
    std::string key = varname + "=";
    const size_t keylen = key.size();
    for (int i = 0; environ[i] != nullptr; ++i) {
        if (strncmp(environ[i], key.c_str(), keylen) == 0) {
            for (int j = i; environ[j] != nullptr; ++j) {
                environ[j] = environ[j + 1];
            }
            return true;
        }
    }
    return false;
}


void UnSetEnvCommand::execute() {
  std::istringstream file(getCmdLine());
  std::vector<std::string> arguments;
  std::string word;
  while (file >> word) {
    arguments.push_back(word);
  }
  if (arguments.size() < 2) {
    std::cerr << "smash error: unsetenv: not enough arguments" << std::endl;
    return;
  }
  for (size_t idx = 1; idx < arguments.size(); idx++) {
    const std::string& varname = arguments[idx];
    if (!eexists(varname)) {
      std::cerr << "smash error: unsetenv: " << varname << " does not exist" << std::endl;
      return;
    }
    if (!remove_var(varname)) {
      std::cerr << "smash error: unsetenv: " << varname << " does not exist" << std::endl;
      return;
    }
  }
}

