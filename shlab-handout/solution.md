
## 实验简介

### 实现目的

在这个实验中，我们需要实现四条指令

![alt text](<solution/屏幕截图 2026-11-10 123522.png>)

我们需要在以下函数中完成指令功能的实现，参考图里也标明了建议的代码行数：

```c
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
```

![alt text](<solution/屏幕截图 2026-11-10 123330.png>)

### 评估

此外，实验包还提供了官方参考程序 tshref 以及 16 个跟踪文件（trace01.txt ~ trace16.txt），用于辅助调试与功能验证。我们可以通过驱动脚本 sdriver.pl 分别运行自己的实现与官方参考，比对输出是否一致：

```bash
# 运行个人实现
./sdriver.pl -t trace01.txt -s ./tsh -a "-p"

# 运行官方参考实现
./sdriver.pl -t trace01.txt -s ./tshref -a "-p"
```

或者使用另一种更简单的办法，进行测试

```bash
make test01

make rtest14
```

### 代码解析

```c



struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */ 
```
job list 的主要结构，包含有以下几种成员，其中 `pid` 和 `jid` 是进程号和作业号。`cmdline` 为解析过后的指令和参数。

`state` 为 `job` 的状态，并且给出了状态的变化规则：

```c
/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */
```

此外还提供了 job 的辅助函数

```C
/* Here are helper routines that we've provided for you */
// 删除 job 操作
int deletejob(struct job_t *jobs, pid_t pid);
void clearjob(struct job_t *job);

// 初始化
void initjobs(struct job_t *jobs);

// 新增 job
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);

// 获取前台 job pid
pid_t fgpid(struct job_t *jobs);

// 通过 pid 或者 jid 获取到 job 指针，没有返回 NULL
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);

// map pid -> jid
int pid2jid(pid_t pid);

// 打印出所有 jobs
void listjobs(struct job_t *jobs);

// 获取最大的 jid
int maxjid(struct job_t *jobs);
```

## 实现流程

### 整体结构

书中零散的分布了 shell 的部分实现，整体框架和对应页数如下：

```c
// CSAPP: P525
void eval(char *cmdline) {

    // 1. parse argv, CSAPP: 526
    bg = parseline(buf, argv)

    // 2. Check if the command is a built-in command
    if (builtin_cmd(argv)){
        // 2.1 if the command is not a built-in command, fork a child and execute the process
        // CSAPP: P543
        // child tasks
        if ((pid = Fork()) == 0)
        {
            setpgid(0, 0);

            sigprocmask(SIG_SETMASK, &prev, NULL);
            // execve error
            if (execve(argv[0], argv, environ) < 0)
            {
                printf("%s: Command not found\n", argv[0]);
                exit(1);
            }
            exit(0);
        }

        // 2.2 add the job into jobs list
        addjob(jobs, pid, bg ? BG : FG, cmdline);

        // 2.3 wait the fg command and skip the bg command
        // CSAPP: P545
        // ...
    }
}
```

### builtin_cmd

- `quit` 指令相当简单，直接匹配到 quit 字符串之后，直接执行 `exit(0)` 即可。

- `jobs` 指令也是相对简单，使用辅助函数 `listjobs` 就可以直接打印出所有的 job 状态，**但是实际中可能出现读写同时操作的情况**，因此我们需要在调用 `listjobs` 的时候屏蔽掉所有的信号防止这种可能的出现。

- `fg` 和 `bg` 在后面进一步阐述

```c
/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv)
{
    if (!strcmp(argv[0], "quit"))
    {
        exit(0);
    }

    // 对用户输入一个 & 做一次静默的容错处理。
    if (!strcmp(argv[0], "&"))
    {
        return 1;
    }

    // 对用户输入一个 & 做一次静默的容错处理。
    if (!strcmp(argv[0], "jobs"))
    {
        sigset_t mask_all, prev_mask;
        sigfillset(&mask_all);

        sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
        listjobs(jobs);
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return 1;
    }

    if (!strcmp(argv[0], "fg") || !strcmp(argv[0], "bg"))
    {
        do_bgfg(argv);
        return 1;
    }

    return 0; /* not a builtin command */
}
```

### 接收到 ctrl c/z （sigint_handler && sigtstp_handler）

如果，我们接收到 ctrl c/z，我们会触发中断，进入到对应的中断处理函数中，此时我们需要根据规则**终止/暂停整个前台作业组**。

因此我们需要使用 `-pid` 表示对整个前台作业组进行操作，使用 `pid` 只代表操作单一进程，是不符合要求的。

```c
/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    pid_t fpid = fgpid(jobs);
    if (fpid > 0)
    {
        // kill(-pid, SIGINT);
        kill(-fpid, sig);
    }

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    pid_t fpid = fgpid(jobs);
    if (fpid > 0)
    {
        // kill(-pid, SIGTSTP);
        kill(-fpid, sig);
    }

    return;
}

```

### 对子进程的回收和状态更新（eval && sigchld_handler）

如果不是内置命令，那我们需要使用 `fork` 和 `execve` 新启一个进程来执行命令。参考 trace 文件的示例，如：

```bash
/bin/echo -e tsh> ./myspin 2 \046
```

我们需要记录这个 job 的相关信息，并且当这个自旋结束之后，还要即时的回收进行的资源，故而我们需要对 `eval` 函数和 `sigchld_handler` 进行修改：

然后在我们进行 `add` 操作的时候需要屏蔽掉所有的信号，防止 `delete` 操作执行在 `add` 操作之前。**具体的例子可以参考书上 P541**

setpgid(0, 0) 操作是为了使生成的子进程是不同的 jobID，防止和主进程在同一个作业组中，导致接收到 SIGINT 信号的时候将主进程也杀死。在文档中我们可以得到提示：

> When you run your shell from the standard Unix shell, your shell is running in the foreground process group. If your shell then creates a child process, by default that child will also be a member of the foreground process group. Since typing `Ctrl+C` sends a `SIGINT` to every process in the foreground group, typing `Ctrl+C` will send a `SIGINT` to your shell, as well as to every process that your shell created, which obviously isn't correct.
> 
> Here is the workaround: After the `fork`, but before the `execve`, the child process should call `setpgid(0, 0)`, which puts the child in a new process group whose group ID is identical to the child’s PID. This ensures that there will be only one process, your shell, in the foreground process group. When you type `Ctrl+C`, the shell should catch the resulting `SIGINT` and then forward it to the appropriate foreground job (or more precisely, the process group that contains the foreground job).


```c
/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline)
{
    char buf[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;
    int bg;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
    {
        printf("%s: the first argv is null\n", cmdline);
        return;
    }

    if (!builtin_cmd(argv))
    {
        sigprocmask(SIG_BLOCK, &mask_one, &prev);
        // child tasks
        if ((pid = Fork()) == 0)
        {
            setpgid(0, 0);

            sigprocmask(SIG_SETMASK, &prev, NULL);
            // execve error
            if (execve(argv[0], argv, environ) < 0)
            {
                printf("%s: Command not found\n", argv[0]);
                exit(1);
            }
            exit(0);
        }

        sigprocmask(SIG_BLOCK, &mask_all, NULL);
        addjob(jobs, pid, bg ? BG : FG, cmdline);
        sigprocmask(SIG_SETMASK, &prev, NULL);

        // parent tasks
        if (!bg)
        {
            waitfg(pid);
        }
        else
        {
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
    }

    return;
}
```

需要注意的是，和书上不同，这里 `waitpid` 需要使用 `WNOHANG | WUNTRACED` 选项，代表非阻塞查询，如果存在需要回收的子进程，那么就会返回对应 pid，否则就是直接返回 0 直接退出 handler，等待下一次信号到来。

如果像书上直接写 0 那么会出现当存在长时间的自旋的时候，导致长时间阻塞等待，导致 shell 卡死。

根据规则，如果接收到 ctrl z 我们需要在 jobs list 中将其状态标记为 ST。

```c
/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig)
{
    int olderrno = errno;
    int status;
    sigset_t mask_all, prev_all;
    pid_t pid;

    sigfillset(&mask_all);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFSIGNALED(status))
        {
            // unsafe function
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        // ctrl z
        else if (WIFSTOPPED(status))
        {
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(jobs, pid);
            if (job != NULL)
            {
                job->state = ST;
            }
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        else if (WIFEXITED(status))
        {
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
    }

    if (pid < 0 && errno != ECHILD)
    {
        unix_error("waitpid error");
    }

    // recovery errno
    errno = olderrno;

    return;
}
```

### fg && bg 执行（do_bgfg）

我们需要对进入到 `do_bgfg` 的参数进行解析，参考 `tshref` 的输出和 writehand 给的提示，我们需要进行一些异常检查，大概有以下检查：

1. fg / bg 是否携带参数
2. pid 和 jid 的格式检查
3. pid / jid 是否存在 jobs
4. bg 操作非阻塞，需打印出对应作业信息

一一对应的格式化输出为：

```c
// 1
printf("%s command requires PID or %%jobid argument\n", argv[0]);

// 2
printf("%s command requires PID or %%jobid argument\n", argv[0]);
printf("%s: argument must be a PID or %%jobid\n", argv[0]);

// 3
printf("(%d): No such process\n", id);

// 4
printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
```

更新状态的时候同样需要屏蔽掉所有的信号，防止状态更新异常。并且，还是需要对作业组进行操作，因此传参应该是传入 `-pid`。

```c
/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    struct job_t *job = NULL;
    char *id_str = argv[1];
    int id;

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    // 更新状态
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    if (id_str == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return;
    }

    // 2. 解析参数是 JID / PID
    // JID
    if (id_str[0] == '%')
    {
        if (!isdigit(id_str[1]))
        {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }

        id = atoi(&id_str[1]);
        job = getjobjid(jobs, id);
        if (job == NULL)
        {
            printf("%%%d: No such job\n", id);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
    }
    // PID
    else
    {
        if (!isdigit(id_str[0]))
        {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
        id = atoi(id_str);
        job = getjobpid(jobs, id);
        if (job == NULL)
        {
            printf("(%d): No such process\n", id);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
    }


    int is_fg = (strcmp(argv[0], "fg") == 0);
    job->state = is_fg ? FG : BG;
    // 如果是后台作业，必须按规范打印作业信息
    if (!is_fg)
    {
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }

    pid_t pid = job->pid;
    // 发送 SIGCONT 唤醒整个进程组
    kill(-pid, SIGCONT);

    sigprocmask(SIG_SETMASK, &prev_all, NULL);


    // 等待 fg 完成
    if (is_fg)
    {
        waitfg(pid);
    }

    return;
}

```

### fg 阻塞等待（waitfg）

我们查询 jobs 中是否存在对应 pid，如果 pid 不存在，说明已经被回收，那么我们就可以停止等待了。

按照 hint 中提示，我们只需使用 `sleep` 处理忙等待就行了。

> In waitfg, use a busy loop around the sleep function.

```c
/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (fgpid(jobs) == pid)
    {
        sleep(1);
    }
}
```

### 完整代码

```c
/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

sigset_t mask_all, mask_one, prev;

/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

// error function
pid_t Fork(void);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */
    sigfillset(&mask_all);
    sigemptyset(&prev);
    sigemptyset(&mask_one);
    sigaddset(&mask_one, SIGCHLD);

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {
        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline)
{
    char buf[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;
    int bg;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
    {
        printf("%s: the first argv is null\n", cmdline);
        return;
    }

    if (!builtin_cmd(argv))
    {
        sigprocmask(SIG_BLOCK, &mask_one, &prev);
        // child tasks
        if ((pid = Fork()) == 0)
        {
            setpgid(0, 0);

            sigprocmask(SIG_SETMASK, &prev, NULL);
            // execve error
            if (execve(argv[0], argv, environ) < 0)
            {
                printf("%s: Command not found\n", argv[0]);
                exit(1);
            }
            exit(0);
        }

        sigprocmask(SIG_BLOCK, &mask_all, NULL);
        addjob(jobs, pid, bg ? BG : FG, cmdline);
        sigprocmask(SIG_SETMASK, &prev, NULL);

        // parent tasks
        if (!bg)
        {
            waitfg(pid);
        }
        else
        {
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
    }

    return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv)
{
    if (!strcmp(argv[0], "quit"))
    {
        exit(0);
    }

    // 对用户输入一个 & 做一次静默的容错处理。
    if (!strcmp(argv[0], "&"))
    {
        return 1;
    }

    // 对用户输入一个 & 做一次静默的容错处理。
    if (!strcmp(argv[0], "jobs"))
    {
        sigset_t mask_all, prev_mask;
        sigfillset(&mask_all);

        sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
        listjobs(jobs);
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return 1;
    }

    if (!strcmp(argv[0], "fg") || !strcmp(argv[0], "bg"))
    {
        do_bgfg(argv);
        return 1;
    }

    return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    struct job_t *job = NULL;
    char *id_str = argv[1];
    int id;

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    // 更新状态
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    if (id_str == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return;
    }

    // 2. 解析参数是 JID / PID
    // JID
    if (id_str[0] == '%')
    {
        if (!isdigit(id_str[1]))
        {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }

        id = atoi(&id_str[1]);
        job = getjobjid(jobs, id);
        if (job == NULL)
        {
            printf("%%%d: No such job\n", id);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
    }
    // PID
    else
    {
        if (!isdigit(id_str[0]))
        {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
        id = atoi(id_str);
        job = getjobpid(jobs, id);
        if (job == NULL)
        {
            printf("(%d): No such process\n", id);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
    }


    int is_fg = (strcmp(argv[0], "fg") == 0);
    job->state = is_fg ? FG : BG;
    // 如果是后台作业，必须按规范打印作业信息
    if (!is_fg)
    {
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }

    pid_t pid = job->pid;
    // 发送 SIGCONT 唤醒整个进程组
    kill(-pid, SIGCONT);

    sigprocmask(SIG_SETMASK, &prev_all, NULL);


    // 等待 fg 完成
    if (is_fg)
    {
        waitfg(pid);
    }

    return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (fgpid(jobs) == pid)
    {
        sleep(1);
    }
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig)
{
    int olderrno = errno;
    int status;
    sigset_t mask_all, prev_all;
    pid_t pid;

    sigfillset(&mask_all);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFSIGNALED(status))
        {
            // unsafe function
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        // ctrl z
        else if (WIFSTOPPED(status))
        {
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(jobs, pid);
            if (job != NULL)
            {
                job->state = ST;
            }
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        else if (WIFEXITED(status))
        {
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
    }

    if (pid < 0 && errno != ECHILD)
    {
        unix_error("waitpid error");
    }

    // recovery errno
    errno = olderrno;

    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    pid_t fpid = fgpid(jobs);
    if (fpid > 0)
    {
        // kill(-pid, SIGINT);
        kill(-fpid, sig);
    }

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    pid_t fpid = fgpid(jobs);
    if (fpid > 0)
    {
        // kill(-pid, SIGTSTP);
        kill(-fpid, sig);
    }

    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");

    return pid;
}
```

## 评估

可以借助脚本直接比较，打印出不同的行数方便检查

```bash
bash testall.sh | tee res.log
```

```bash
for i in $(seq -w 1 16); do
    echo "=== Testing trace$i.txt ==="
    ./sdriver.pl -t trace$i.txt -s ./tsh -a "-p" > /tmp/tsh.out
    ./sdriver.pl -t trace$i.txt -s ./tshref -a "-p" > /tmp/tshref.out
    diff -u <(sed -E 's/\([0-9]+\)/(PID)/g' /tmp/tsh.out) \
            <(sed -E 's/\([0-9]+\)/(PID)/g' /tmp/tshref.out)
    if [ $? -eq 0 ]; then
        echo "trace$i PASSED"
    fi
done
```

``` log
=== Testing trace01.txt ===
trace01 PASSED
=== Testing trace02.txt ===
trace02 PASSED
=== Testing trace03.txt ===
trace03 PASSED
=== Testing trace04.txt ===
trace04 PASSED
=== Testing trace05.txt ===
trace05 PASSED
=== Testing trace06.txt ===
trace06 PASSED
=== Testing trace07.txt ===
trace07 PASSED
=== Testing trace08.txt ===
trace08 PASSED
=== Testing trace09.txt ===
trace09 PASSED
=== Testing trace10.txt ===
trace10 PASSED
=== Testing trace11.txt ===
--- /dev/fd/63	2026-11-10 14:37:12.017425832 +0800
+++ /dev/fd/62	2026-11-10 14:37:12.017425832 +0800
@@ -51,6 +51,6 @@
  894065 pts/2    Sl+    0:28 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools-srv 586070 {E6330E5E-E945-4080-8FCC-3E9D985F108A}
  936975 pts/8    S+     0:00 bash testall.sh
  936976 pts/8    S+     0:00 tee res.log
- 937838 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace11.txt -s ./tsh -a -p
- 937839 pts/8    S+     0:00 ./tsh -p
- 937859 pts/8    R      0:00 /bin/ps a
+ 937860 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace11.txt -s ./tshref -a -p
+ 937861 pts/8    S+     0:00 ./tshref -p
+ 937910 pts/8    R      0:00 /bin/ps a
=== Testing trace12.txt ===
--- /dev/fd/63	2026-11-10 14:37:16.051704579 +0800
+++ /dev/fd/62	2026-11-10 14:37:16.051704579 +0800
@@ -21,7 +21,7 @@
  585600 pts/2    S+     0:00 sh -c "$VSCODE_WSL_EXT_LOCATION/scripts/wslServer.sh" c4453f8ea430795f941c1b2ff6c9321135a74a02 stable code-server .vscode-server --host=127.0.0.1 --port=0 --connection-token=1761344009-2230041557-177547242-1149610843 --use-host-proxy --without-browser-env-var --disable-websocket-compression --accept-server-license-terms --telemetry-level=all
  585601 pts/2    S+     0:00 sh /mnt/c/Users/86188/.vscode/extensions/ms-vscode-remote.remote-wsl-1.104.7/scripts/wslServer.sh c4453f8ea430795f941c1b2ff6c9321135a74a02 stable code-server .vscode-server --host=127.0.0.1 --port=0 --connection-token=1761344009-2230041557-177547242-1149610843 --use-host-proxy --without-browser-env-var --disable-websocket-compression --accept-server-license-terms --telemetry-level=all
  585607 pts/2    S+     0:00 sh /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/bin/code-server --host=127.0.0.1 --port=0 --connection-token=1761344009-2230041557-177547242-1149610843 --use-host-proxy --without-browser-env-var --disable-websocket-compression --accept-server-license-terms --telemetry-level=all
- 585611 pts/2    Sl+    0:27 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/server-main.js --host=127.0.0.1 --port=0 --connection-token=1761344009-2230041557-177547242-1149610843 --use-host-proxy --without-browser-env-var --disable-websocket-compression --accept-server-license-terms --telemetry-level=all
+ 585611 pts/2    Rl+    0:27 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/server-main.js --host=127.0.0.1 --port=0 --connection-token=1761344009-2230041557-177547242-1149610843 --use-host-proxy --without-browser-env-var --disable-websocket-compression --accept-server-license-terms --telemetry-level=all
  585633 pts/2    Sl+    0:09 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=ptyHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154
  585676 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=fileWatcher
  585739 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=agentHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154 --user-data-dir /home/liurun/.vscode-server/data --telemetry-level all
@@ -53,8 +53,8 @@
  894065 pts/2    Sl+    0:28 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools-srv 586070 {E6330E5E-E945-4080-8FCC-3E9D985F108A}
  936975 pts/8    S+     0:00 bash testall.sh
  936976 pts/8    S+     0:00 tee res.log
- 937916 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace12.txt -s ./tsh -a -p
- 937917 pts/8    S+     0:00 ./tsh -p
- 937922 pts/8    T      0:00 ./mysplit 4
- 937923 pts/8    T      0:00 ./mysplit 4
- 937941 pts/8    R      0:00 /bin/ps a
+ 937942 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace12.txt -s ./tshref -a -p
+ 937943 pts/8    S+     0:00 ./tshref -p
+ 937945 pts/8    T      0:00 ./mysplit 4
+ 937946 pts/8    T      0:00 ./mysplit 4
+ 937976 pts/8    R      0:00 /bin/ps a
=== Testing trace13.txt ===
--- /dev/fd/63	2026-11-10 14:37:24.106221913 +0800
+++ /dev/fd/62	2026-11-10 14:37:24.106221913 +0800
@@ -25,7 +25,7 @@
  585633 pts/2    Sl+    0:09 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=ptyHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154
  585676 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=fileWatcher
  585739 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=agentHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154 --user-data-dir /home/liurun/.vscode-server/data --telemetry-level all
- 585788 pts/2    Sl+   11:21 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node --dns-result-order=ipv4first /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=extensionHost --transformURIs --useHostProxy=true
+ 585788 pts/2    Sl+   11:22 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node --dns-result-order=ipv4first /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=extensionHost --transformURIs --useHostProxy=true
  586070 pts/2    Sl+    1:27 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools
  586134 pts/7    Ss+    0:00 /bin/sh -c cd '/home/liurun/courses/csapp/lab/shlab-handout' && /bin/sh
  586135 pts/7    S+     0:00 /bin/sh
@@ -53,11 +53,11 @@
  894065 pts/2    Sl+    0:28 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools-srv 586070 {E6330E5E-E945-4080-8FCC-3E9D985F108A}
  936975 pts/8    S+     0:00 bash testall.sh
  936976 pts/8    S+     0:00 tee res.log
- 937982 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace13.txt -s ./tsh -a -p
- 937983 pts/8    S+     0:00 ./tsh -p
- 937985 pts/8    T      0:00 ./mysplit 4
- 937986 pts/8    T      0:00 ./mysplit 4
- 938024 pts/8    R      0:00 /bin/ps a
+ 938055 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace13.txt -s ./tshref -a -p
+ 938056 pts/8    S+     0:00 ./tshref -p
+ 938058 pts/8    T      0:00 ./mysplit 4
+ 938060 pts/8    T      0:00 ./mysplit 4
+ 938101 pts/8    R      0:00 /bin/ps a
 tsh> fg %1
 tsh> /bin/ps a
     PID TTY      STAT   TIME COMMAND
@@ -79,7 +79,7 @@
  585633 pts/2    Sl+    0:09 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=ptyHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154
  585676 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=fileWatcher
  585739 pts/2    Sl+    0:06 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=agentHost --logsPath /home/liurun/.vscode-server/data/logs/20261109T203154 --user-data-dir /home/liurun/.vscode-server/data --telemetry-level all
- 585788 pts/2    Sl+   11:21 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node --dns-result-order=ipv4first /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=extensionHost --transformURIs --useHostProxy=true
+ 585788 pts/2    Sl+   11:22 /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/node --dns-result-order=ipv4first /home/liurun/.vscode-server/bin/c4453f8ea430795f941c1b2ff6c9321135a74a02/out/bootstrap-fork --type=extensionHost --transformURIs --useHostProxy=true
  586070 pts/2    Sl+    1:27 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools
  586134 pts/7    Ss+    0:00 /bin/sh -c cd '/home/liurun/courses/csapp/lab/shlab-handout' && /bin/sh
  586135 pts/7    S+     0:00 /bin/sh
@@ -107,6 +107,6 @@
  894065 pts/2    Sl+    0:28 /home/liurun/.vscode-server/extensions/ms-vscode.cpptools-1.35.6-linux-x64/bin/cpptools-srv 586070 {E6330E5E-E945-4080-8FCC-3E9D985F108A}
  936975 pts/8    S+     0:00 bash testall.sh
  936976 pts/8    S+     0:00 tee res.log
- 937982 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace13.txt -s ./tsh -a -p
- 937983 pts/8    S+     0:00 ./tsh -p
- 938054 pts/8    R      0:00 /bin/ps a
+ 938055 pts/8    S+     0:00 /usr/bin/perl ./sdriver.pl -t trace13.txt -s ./tshref -a -p
+ 938056 pts/8    S+     0:00 ./tshref -p
+ 938131 pts/8    R      0:00 /bin/ps a
=== Testing trace14.txt ===
trace14 PASSED
=== Testing trace15.txt ===
trace15 PASSED
=== Testing trace16.txt ===
trace16 PASSED
```

目前没有看出有什么明显的错误，差异同样主要来自：
- tsh 和 tshref 的进程信息。
- 不同的进程 PID。
- /bin/ps a 显示的 CPU 时间和进程状态。
- 测试期间动态变化的系统进程信息。
