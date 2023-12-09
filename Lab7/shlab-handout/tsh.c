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

/**
 * For debugging purpose, Error handling wrapper
 * Defined as macro function.
 */

#define check_error(cond, from) ({ \
    if ((cond))                    \
    {                              \
        unix_error(from " error"); \
    }                              \
})

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

    char *argv[MAXARGS]; /* Arguments parsed */

    pid_t process_id;

    int background_job = parseline(cmdline, argv); /* Save parsed args, and return how job should executed (fore / back) */

    /* Handling for empty line */
    if (argv[0] == NULL)
    {
        return;
    }

    int is_builtin = builtin_cmd(argv); /* Run the builtin commands */

    sigset_t sigset;

    if (!is_builtin)
    {

        /* Initialize the signal set */
        check_error(sigemptyset(&sigset) < 0, "sigemptyset");

        /* Add SIGCHLD, SIGINT, SIGTSTP to signal set */
        check_error(sigaddset(&sigset, SIGCHLD), "SIGCHLD sigaddset");
        check_error(sigaddset(&sigset, SIGINT), "SIGINT sigaddset");
        check_error(sigaddset(&sigset, SIGTSTP), "SIGTSTP sigaddset");

        /**
         * BLOCK the SIGCHLD, SIGINT, SIGTSTP signal before forking
         * UNBLOCK the such signals for both parent and child process.
         *
         * fork() should be considered as critical section.
         * Before their handler are setted up, any signal accepting to
         * child (or parent) process might be undefined behavior.
         */
        check_error(sigprocmask(SIG_BLOCK, &sigset, NULL) < 0, "sigprocmask");

        process_id = fork();
        check_error(process_id < 0, "fork");

        /* IF THE PROCESS IS CHILD PROCESS */
        if (process_id == 0)
        {

            /* Unblock the signal SIGCHLD, SIGINT, SIGTSTP for child process. */
            sigprocmask(SIG_UNBLOCK, &sigset, NULL);

            /**
             * Make new pgroup for child process, to reap child of child process
             * properly, by negative PID to `kill`.
             */
            check_error(setpgid(0, 0) < 0, "setpgid");

            execve(argv[0], (char *const *)argv, (char *const *)environ);

            /**
             * PROCESS OVERWRITTEN - NEVER REACHED AFTER HERE!
             * Except argv[0] in `execve` is invalid command.
             */
            printf("%s : Command not found\n", argv[0]);
            exit(0);
        }
        /* END OF CHILD PROCESS */

        /* IF THE PROCESS IS PARENT PROCESS */

        /* Add job with pid, job running location, command line. */
        if (background_job)
        {
            /**
             * Add job (child) as background job.
             *
             * Unblock the signal SIGCHLD, SIGINT, SIGTSTP for parent process.
             *
             * Prompt the message of background job.
             */
            addjob(jobs, process_id, BG, cmdline);
            sigprocmask(SIG_UNBLOCK, &sigset, NULL);
            printf("[%d] (%d) %s", getjobpid(jobs, process_id)->jid, process_id, cmdline);
        }
        else
        {
            /**
             * Add job (child) as foreground job.
             *
             * Unblock the signal SIGCHLD, SIGINT, SIGTSTP for parent process.
             *
             * Wait for foreground running child process.
             */
            addjob(jobs, process_id, FG, cmdline);
            sigprocmask(SIG_UNBLOCK, &sigset, NULL);
            waitfg(process_id);
        }
        /* END OF PARENT PROCESS */
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
    const char *command = argv[0];

    /**
     * List of builtin commands for tsh (on `shlab.pdf` Pg. 4)
     * - quit                  : No argument. Terminates the tsh.
     * - jobs                  : No argument. List all background jobs.
     * - bg <job : PID or JID> : Restart the job given argument <job> by SIGCONT, run in background.
     * - fg <job : PID or JID> : Restart the job given argument <job> by SIGCONT, run in foreground.
     */

    if (!strcmp(command, "quit"))
    {
        exit(0);
        /* NOT REACHED AFTER EXIT */
    }
    else if (!strcmp(command, "jobs"))
    {
        listjobs(jobs);
        return 1;
    }
    else if (!strcmp(command, "bg"))
    {
        do_bgfg(argv);
        return 1;
    }
    else if (!strcmp(command, "fg"))
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
    struct job_t *job;

    int job_id;
    pid_t process_id;

    /**
     * Argument Parsing Part.
     *
     * Argument might be a [Job ID] or [Process ID].
     * If argument is not given, or wrong format, then return.
     */

    /* Neither Process ID nor Job ID given*/
    if (argv[1] == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    /* Given argument is Job ID*/
    if (argv[1][0] == '%')
    {
        job_id = atoi(&argv[1][1]);
        job = getjobjid(jobs, job_id);

        /* Validate job */
        if (job == NULL)
        {
            printf("%s : No such job\n", argv[1]);
            return;
        }

        process_id = job->pid;
    }

    /* Given argument is Process ID*/
    else if (isdigit(argv[1][0]))
    {
        process_id = atoi(argv[1]);

        /* Validate process */
        if (getjobpid(jobs, process_id) == NULL)
        {
            printf("(%s) : No such process\n", argv[1]);
            return;
        }

        job_id = pid2jid(process_id);
        job = getjobjid(jobs, job_id);
    }

    /* Given argument is unknown format */
    else
    {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }

    /**
     * Job Changing Part
     *
     * If job was stopped: via signal SIGCONT, job can run on foreground or background.
     * If job was running on background: via signal SIGCONT, job can run on foreground.
     *
     * At this point, variable `struct job_t *job` must reference valid job
     * on array `struct job_t jobs[]`
     */

    if (!strcmp(argv[0], "fg"))
    {
        job->state = FG;            /* Set the job state as foreground */
        kill(-process_id, SIGCONT); /* Send the signal. */
        waitfg(job->pid);           /* Wait until job terminates.*/
    }
    else if (!strcmp(argv[0], "bg"))
    {
        job->state = BG;                                          /* Set the job state as background */
        kill(-process_id, SIGCONT);                               /* Send the signal. */
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline); /* Print the background job prompt. */
    }
    return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (pid == fgpid(jobs))
    {
        sleep(1);
    }
    return;
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
    int child_code;
    pid_t process_id;

    /**
     * Return of `waitpid`, when WNOHANG flag has turned on is following:
     *  - `pid` (> 0) : If there is zombified child process to be reaped by parent process.
     *  - 0           : If there are no more child process to be reaped.
     *  - -1          : If there was error to fetch.
     *
     * In while loop, we should reap every 'zombified' child process
     * that are waiting for parent's handling,
     * and escape loop when no more child processes are waiting to be reaped.
     *
     * Therefore, condition should be `waitpid() > 0` for this `while` loop.
     */
    while ((process_id = waitpid(-1, &child_code, WNOHANG | WUNTRACED)) > 0)
    {
        /* Had child process stopped? */
        if (WIFSTOPPED(child_code))
        {
            /* Set (stopped) foreground job status as Stopped. */
            getjobpid(jobs, process_id)->state = ST;
            printf("Job [%d] (%d) stopped by signal %d\n", getjobpid(jobs, process_id)->jid, process_id, WSTOPSIG(child_code));
            return;
        }

        /* Had child process signalled? */
        else if (WIFSIGNALED(child_code))
        {
            /* Job has terminated, no longer able to run again. */
            printf("Job [%d] (%d) terminated by signal %d\n", getjobpid(jobs, process_id)->jid, process_id, WTERMSIG(child_code));
            deletejob(jobs, process_id);
        }

        /* Had child process normally exited? */
        else if (WIFEXITED(child_code))
        {
            /* Job has exited, no longer able to run. */
            deletejob(jobs, process_id);
        }
    }

    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    pid_t fgjob_id = fgpid(jobs);

    /* Is valid PID and is it running on foreground? */
    if (fgjob_id <= 0 || getjobpid(jobs, fgjob_id)->state != FG)
    {
        return;
    }

    /* Use negative PID to send signal to every process in such group */
    check_error(kill(-fgjob_id, sig) < 0, "SIGINT kill error");
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    pid_t fgjob_id = fgpid(jobs);

    /* Is valid PID and is it running on foreground? */
    if (fgjob_id <= 0 || getjobpid(jobs, fgjob_id)->state != FG)
    {
        return;
    }

    /* Use negative PID to send signal to every process in such group */
    check_error(kill(-fgjob_id, sig) < 0, "SIGTSTP kill error");
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
