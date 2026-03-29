#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string.h>
#include <map>
#include <unordered_map>
#include <regex>
#include <unordered_set>
#include <list>
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define PATH_MAX (4096)
class Command {
protected:
    pid_t pid;
    const char* cmd_line;
public:
    Command(const char *cmd_line): pid(-1), cmd_line(cmd_line) {}

    virtual ~Command() = default;

    virtual void execute() = 0;
    pid_t getPid() const { return pid; }
    std::string getCmdLine() const { return cmd_line; }

};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
private:
    char* cmd;
public:
    ExternalCommand(const char *cmd_line) : Command(cmd_line) {
        this->cmd = strdup(cmd_line);
    }
    void executeChild(const std::string& cmd_s);
    virtual ~ExternalCommand() {}

    void execute() override;
};
class RedirectionCommand : public Command {
private:
    std::string cmd;
    std::string file;
    bool app;

public:
    explicit RedirectionCommand(const char *cmd_line);
    int openRedirectFile() const ;
    virtual ~RedirectionCommand() {}

    void execute() override;
};
class PipeCommand : public Command {
private:
    std::string cmd1;
    std::string cmd2;
    bool stderr_pipe;

public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};
class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line): Command(cmd_line) {
    }

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};
class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line): Command(cmd_line) {
    }

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};
class USBInfoCommand : public Command {
public:
    USBInfoCommand(const char *cmd_line): Command(cmd_line) {
    }

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};
class ChangeDirCommand : public BuiltInCommand {
    char* lastPwd;
public:
    ChangeDirCommand(const char *cmd_line, char* plastPwd) : BuiltInCommand(cmd_line), lastPwd(plastPwd) {}
    const char* getTargetPath(char* args[], int argc, const char* lastPwd) ;
    virtual ~ChangeDirCommand() {}

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line): BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() {}

    void execute() override;

};

class JobsList;

class QuitCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    void killAllJobsAndPrint();
    virtual ~QuitCommand() {}
    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int jobId;
        pid_t pid;
        bool isStopped;
        std::string cmdLine;

        JobEntry(Command* cmd, int jobId, bool isStopped, pid_t pid)
            : jobId(jobId), pid(pid), isStopped(isStopped), cmdLine(cmd->getCmdLine()) {
                if (cmdLine.empty()) {
                    throw std::logic_error("Command line cannot be empty");
                }
            }

        JobEntry() : jobId(0), pid(-1), isStopped(false), cmdLine("") {}
    };

    std::map<int, JobEntry> jobs;

public:
    JobsList() = default;

    ~JobsList() = default;

    void addJob(Command *cmd, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);
};

class JobsCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    bool parseKillArgs(const std::vector<std::string>& tokens,int& signalNum, int& jobId) ;
    virtual ~KillCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    JobsList::JobEntry* getJobForFg(const std::vector<std::string>& tokens, int& jobId) ;
    virtual ~ForegroundCommand() {}

    void execute() override;
};



class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    bool parseAliasCommand(const std::string& cmdStr,std::string& name,std::string& command);
    virtual ~AliasCommand() {}

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~UnAliasCommand() {}

    void execute() override;
};

class SmallShell {
private:
    std::string prompt;
    pid_t smash_pid;
    pid_t fore_pid;
    char *lastPwd;
    SmallShell();

public:
    JobsList jobsList;

    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete;
    void operator=(SmallShell const &) = delete;
    static SmallShell &getInstance()
    {
        static SmallShell instance;

        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    void setPrompt(const std::string &new_prompt);
    const std::string& getPrompt() const;

    pid_t getSmashPid() const;
    void setForegroundPID(pid_t pid);
    pid_t getForePID() const;

    char* getLastPwd() const;
    void setLastPwd(const char* newPwd);
};


class ChpromptCommand : public BuiltInCommand {
public:
    ChpromptCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}
    void execute() override;
};
class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line): BuiltInCommand(cmd_line) {
    }
    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line): BuiltInCommand(cmd_line) {
    }

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};

class AliasManager {
private:
    std::list<std::pair<std::string, std::string>> al_List;
    std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator> aliases;
    std::unordered_set<std::string> taken_words = {
        "cd", "pwd", "showpid",
        "jobs", "fg", "kill", "quit",
        "alias", "unalias",
        "sysinfo", "whoami", "usbinfo",
        ">", ">>", "|", "|&"
    };

    AliasManager() = default;

public:
    static AliasManager& getInstance() {
        static AliasManager instance;
        return instance;
    }
    void printAliases() const;
    bool if_Res(const std::string& name) const;
    bool insertAlias(const std::string& name, const std::string& command);
    bool remAlias(const std::string& name);
    std::string getAlias(const std::string& name) const;


    AliasManager(const AliasManager&) = delete;
    AliasManager& operator=(const AliasManager&) = delete;
};

#endif