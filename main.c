#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFF_SIZE 10240
#define MY_CUSTOM_EXIT_STATUS 0x180
#define PRINT_HELP_ON_WRONG_INPUT

// For Windows
//#if (defined(_WIN32) || defined(__WIN32__))
//#define mkdir(A, B) mkdir(A)
//
//#ifndef S_IFSOCK
//#define S_IFSOCK 0140000
//#endif
//#ifndef S_IFLNK
//#define S_IFLNK 0120000
//#endif
//
//#endif
//
//#if (defined(_WIN32) || defined(__WIN32__))
//
//#include <windows.h>
//#include <winbase.h>
//
//int m_get_type(const char *buf);
//
//int symlink(const char *target, const char *linkpath) {
//    int type = m_get_type(target);
//    if (type == S_IFDIR) { // Target is a directory
//        // FAIL
//        if (!CreateSymbolicLinkA(linkpath, target, SYMBOLIC_LINK_FLAG_DIRECTORY))
//            return -1;
//    } else if (type == S_IFREG) { // Target is a file
//        // FAIL
//        if (!CreateSymbolicLinkA(linkpath, target, 0))
//            return -1;
//    } else
//        return -1; // target not known
//    return 0;
//}
//
//#endif
// End for Windows

/* -------------------------------------- FUNCTION DEFINITIONS --------------------------------------- */

void parseCommand(unsigned argc, char *buf, const char *maxBuf);

void doPrint(unsigned argc, char *buf, const char *maxBuf, const char *stringToPrint);

void m_help();
// -------------------------------------- FUNCTION DEFINITIONS --------------------------------------- */

/* ------------------------------------------- GLOBAL VARS ------------------------------------------- */

FILE *fout_TEST = NULL; // For debugging
int pid_status;
int isChild; // Used to determine if we should spawn a new process or not (if this is already a child process)
// ------------------------------------------- GLOBAL VARS ------------------------------------------- */

/* --------------------------------------------- HELPERS --------------------------------------------- */

// return a pointer to the first non-space char in buf
const char *jumpWhitespaces(const char *buf) {
    while (isspace(*buf)) // jump over a whitespace
        ++buf;

    return buf;
}

// Does buf start with c?
int startsWith(const char *buf, char c) {
    buf = jumpWhitespaces(buf);
    return *buf == c;
}

// e.g.
// "create    tip nume t     d_"
// "create_   tip_nume_t_    d_"  // _ is \0
void parseBuf(char *buf,
              unsigned *argc1, char **buf1, const char **maxBuf1,
              unsigned *argc2, char **buf2, const char **maxBuf2) {
    *argc1 = 0;
    *argc2 = 0;
    unsigned *argc = argc1; // Start with argc1

    buf = (char *) jumpWhitespaces(buf); // Jump if there are whitespaces at the start of the buffer
    if (startsWith(buf, '|')) {
        fprintf(stderr, "-bash: syntax error near unexpected token `|'\n");
        return;
    }
    *buf1 = buf; // The first buffer starts after those jumped whitespaces

//    char *last0 = NULL;
    char *lastChar = NULL;
    char hadSpace = 1, hadPipe = 0;
    while (*buf) {
        if (*buf == '|') {
            if (hadPipe || *(buf + 1) == '\0') {
                if(hadPipe)
                    fprintf(stderr, "ERROR: this shell doesn't support more than 1 PIPEs\n");
                else
                    fprintf(stderr, "ERROR: No arguments after PIPE\n");

                *argc1 = 0;
                *argc2 = 0;
                *buf1 = NULL;
                *buf2 = NULL;
                *maxBuf1 = NULL;
                *maxBuf2 = NULL;
                return;
            }
            *buf = '\0'; // To know when to end command1

            hadSpace = 1; // Consider '|' as space as well
            hadPipe = 1;
            *maxBuf1 = lastChar + 1; // Set the end of the first one to be after the lastChar placed before this |

            buf = (char *) jumpWhitespaces(buf + 1); // Jump over the possible starting whitespaces
            *buf2 = buf; // The second buffer starts after those jumped whitespaces

            argc = argc2; // Switched argc

            // TEST_PRINT
//            doPrint(*argc1, *buf1, *maxBuf1, "command1: ");
//            fprintf(fout_TEST, "buf2=%s; maxBuf1_c=%c\n", *buf2, **maxBuf1);
//            fflush(fout_TEST);
        } else if (isspace(*buf)) {
            hadSpace = 1;
//            last0 = buf;

            *buf = '\0';
            buf = (char *) jumpWhitespaces(buf + 1); // Jump over the rest of the whitespaces
        } else {
            if (hadSpace) {
                hadSpace = 0;
                ++(*argc); // Increase only the first time (we weren't guaranteed to have arguments)
            }

            lastChar = buf;
            ++buf;
        }
    }

    if (*maxBuf1 == NULL) {
        *maxBuf1 = lastChar + 1; // including the last '\0'
    } else if (hadPipe) {
        *maxBuf2 = lastChar + 1; // including the last '\0'
    }

    // TEST_PRINT
//    doPrint(*argc1, *buf1, *maxBuf1, "command1: ");
//    doPrint(*argc2, *buf2, *maxBuf2, "command2: ");
}

// e.g.
// "create_   tip_nume_t_    d_"
// "tip_nume_t_    d_"
// FAILED when return points towards 0
const char *getNextArg(const char *buf, const char *maxBuf) {
    for (; *buf; ++buf);
    if (buf == maxBuf)
        return buf; // The last one, nothing afterwards, end with '\0'
    if (buf > maxBuf) {
        fprintf(stderr, "buf > maxBuf; maxBuf is not calculated correctly!!\n");
        exit(EXIT_FAILURE);
    }

    return jumpWhitespaces(buf + 1);
}

// Allocate an array of strings, and call getNextArg repeatedly and fill the array up
char *const *getArgv(unsigned argc, const char *buf, const char *maxBuf) {
    if (argc <= 0)
        return NULL;

    const char **argv = malloc(sizeof(char *) * (argc + 1));
    if (argv == NULL) {
        perror("getArgv - Couldn't allocate memory!!\n");
        exit(EXIT_FAILURE);
    }

    const char *arg;
    unsigned n = 0;

    argv[n++] = buf;
    while (*(arg = getNextArg(buf, maxBuf))) {
        argv[n++] = arg;
        buf = arg;
    }
    argv[n] = NULL; // last one must be NULL (can't place the last "arg" here, because that one is '\0')

    return (char *const *) argv;
}

// For debugging
void checkOpenFile() {
    if (fout_TEST == NULL) { // Not yet opened (pointer copied to processes, so it's okay)
        fout_TEST = fopen("shell.log", "w");

        if (fout_TEST == NULL) {
            perror("checkOpenFile - couldn't open file");
            exit(EXIT_FAILURE);
        }
    }
}

// For debugging
void doPrint(unsigned argc, char *buf, const char *maxBuf, const char *stringToPrint) {
    checkOpenFile();

    fprintf(fout_TEST, "%s", stringToPrint);

    fprintf(fout_TEST, "argc=%u; ", argc);
    if (argc > 0) {
        char *const *argv = getArgv(argc, buf, maxBuf);
        if (argv == NULL) {
            fprintf(fout_TEST, "argv=NULL");
            return;
        }

        fprintf(fout_TEST, "argv= ");
        for (; *argv != NULL; ++argv)
            fprintf(fout_TEST, "_%s_", *argv);
    }
    if (buf && maxBuf) {
        if (maxBuf > buf)
            fprintf(fout_TEST, " {[%p, %p], [%c, %c]}", buf, maxBuf, *buf, *(maxBuf - 1));
        else
            fprintf(fout_TEST, " {[%p, %p], buf < maxBuf}", buf, maxBuf);
    }
    putc('\n', fout_TEST);

    fflush(fout_TEST);
}

// Helper function for checking if a condition is OK for calling a command (argc conditions)
int canCallConditionedCommand(int isConditionOk, const char *failureMsg) {
    if (isConditionOk)
        return 1;

    pid_status = MY_CUSTOM_EXIT_STATUS;
    fprintf(stderr, "%s", failureMsg);
#ifdef PRINT_HELP_ON_WRONG_INPUT
    m_help();
#endif

    return 0;
}

// perror with an extra
void perrorExtra(const char *format, const char *extra) {
    char s[BUFF_SIZE];
    if (snprintf(s, BUFF_SIZE, format, extra) > 0)
        perror(s);
    else
        perror(format);
}
// --------------------------------------------- HELPERS --------------------------------------------- */


/* ---------------------------------------- BUILT-IN COMMANDS ---------------------------------------- */

void m_help() {
    printf("This is a shell that can do the following:\n"
           "1. A prompt functionality\n"
           "2. Execute commands synchronously\n"
           "3. Built-in commands:\n"
           "   - help         -> displays this message\n"
           "   - exit         -> exits the shell\n"
           "   - cd <DIR>     -> changes the current directory\n"
           "   - pwd          -> prints the current directory\n"
           "   - type <ENTRY> -> displays the type of ENTRY (absolute/relative path)\n"
           "   - create <TYPE> <NAME> [TARGET] [DIR]{.}\n"
           "      - creates a new entry of the given TYPE, where TYPE may have one of the following values:\n"
           "            -> -f  (regular file) -> create -f <NAME> [DIR]{.}\n"
           "            -> -l  (symlink)      -> create -l <NAME> <TARGET> [DIR]{.}\n"
           "            -> -d  (directory)    -> create -d <NAME> [DIR]{.}\n"
           "4. run UNIX commands:\n"
           "   - <COMMAND> [ARG1 ARG2 … ] will execute the given COMMAND with an arbitrary number of arguments\n"
           "   - status -> displays the exit status of the previously executed command\n"
           "5. Support for connecting two commands using one pipe (e.g. ls -l -a | sort)\n"
           "\n");
}

void m_chdir(const char *buf) {
    const char *path = jumpWhitespaces(buf + 3); // +3 because we first jump over "cd "
    if (chdir(path) == -1) {
        pid_status = MY_CUSTOM_EXIT_STATUS;
        perrorExtra("ERROR cd \"%s\"", path);
    }
}

void m_pwd() {
    char *cwd;
    if ((cwd = getcwd(NULL, 0)) != NULL) {
        printf("%s\n", cwd);
        free(cwd);
    } else {
        pid_status = MY_CUSTOM_EXIT_STATUS;
        perror("ERROR pwd");
    }
}

int m_get_type(const char *buf) {
    struct stat sb;

    // QUESTION: Should use lstat(), or stat() here?
    const char *file = jumpWhitespaces(buf + 5); // +5 because we first jump over "type "
    if (lstat(file, &sb) == -1) {
        pid_status = MY_CUSTOM_EXIT_STATUS;
        perrorExtra("ERROR type \"%s\"", file);
        return -1;
    }
    return (int) (sb.st_mode & S_IFMT);
}

void m_print_type(char *buf) {
    int type = m_get_type(buf);
    if (type != -1) {
        switch (type) {
            case S_IFBLK:
                printf("block device\n");
                break;
            case S_IFCHR:
                printf("character device\n");
                break;
            case S_IFDIR:
                printf("directory\n");
                break;
            case S_IFIFO:
                printf("FIFO/pipe\n");
                break;
            case S_IFLNK:
                printf("symlink\n");
                break;
            case S_IFREG:
                printf("regular file\n");
                break;
            case S_IFSOCK: // This is the max value, which is 0xC000
                printf("socket\n");
                break;
            default:
                fprintf(stderr, "unknown?\n");
                break;
        }
    }
}

void m_create(unsigned argc, char *buf, const char *maxBuf) {
    // TODO: Maybe remove extra '/' from path
//    -> -f  (regular file) -> create -f <NAME> [DIR]{.}\n"
//    -> -l  (symlink)      -> create -l <NAME> <TARGET> [DIR]{.}\n"
//    -> -d  (directory)    -> create -d <NAME> [DIR]{.}\n"

    // Get the first 2 params (that should be present for all the commands)
    char *typeBuf = (char *) jumpWhitespaces(buf + 7); // +7 because we first jump over "create "
    char *nameBuf = (char *) getNextArg(typeBuf, maxBuf); // TODO: If '/' in name, create directories until that point

    // Do some checks to make sure the given commands are okay
    if (!(*typeBuf) || !(*nameBuf)) {
        fprintf(stderr, "No type/name provided\n\n");
#ifdef PRINT_HELP_ON_WRONG_INPUT
        m_help();
#endif
        return;
    }

    int isLink = !strcmp(typeBuf, "-l");
    int isFile = !strcmp(typeBuf, "-f");
    int isDir = !strcmp(typeBuf, "-d");

    if (!isLink && !isFile && !isDir) {
        fprintf(stderr, "File type not supported: %s\n\n", typeBuf);
#ifdef PRINT_HELP_ON_WRONG_INPUT
        m_help();
#endif
        return;
    }
    if (isLink) {
        if (argc < 4 || argc > 5) { // Symlinks have argc={4, 5}
            fprintf(stderr, "When creating symlinks, you may only send 3/4 arguments\n\n");
#ifdef PRINT_HELP_ON_WRONG_INPUT
            m_help();
#endif
            return;
        }
    } else {
        if (argc < 3 || argc > 4) { // Files and dirs have argc={3, 4}
            fprintf(stderr, "When creating files/directories, you may only send 2/3 arguments\n\n");
#ifdef PRINT_HELP_ON_WRONG_INPUT
            m_help();
#endif
            return;
        }
    }

    // Get the next 1/2 params
    char *currentArg = nameBuf;
    char *targetBuf = NULL;
    if (isLink) {
        targetBuf = (char *) getNextArg(currentArg, maxBuf);
        currentArg = targetBuf;
    }
    char *dirParam = (char *) getNextArg(currentArg, maxBuf);

    // Init path buffer
    // dirPath + '/' + name + make_sure (dirParam can be of length 0)
    const unsigned pathSize = strlen(dirParam) + 1 + strlen(nameBuf) + 10;
    char completePathBuf[pathSize];

    // Add the current directory first then
    if (*dirParam == '\0') {
        strcpy(completePathBuf, "."); // the +10 helps here as well
    } else {
        strcpy(completePathBuf, dirParam); // strlen(dirParam)
    }

    // Add the filename to the directory
    strcat(completePathBuf, "/"); // + 1
    strcat(completePathBuf, nameBuf); // + strlen(nameBuf)

    if (isFile) { // Create a file
        int fp;
        if ((fp = open(completePathBuf, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
            pid_status = MY_CUSTOM_EXIT_STATUS;
            perrorExtra("Could not create file with path \"%s\"", completePathBuf);
            return;
        }
        if (close(fp) == -1) {
            pid_status = MY_CUSTOM_EXIT_STATUS;
            perrorExtra("Could not close file with path \"%s\"", completePathBuf);
            return;
        }
    } else if (isDir) { // Create a dir
        if (mkdir(completePathBuf, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
            pid_status = MY_CUSTOM_EXIT_STATUS;
            perrorExtra("Could not create directory with path \"%s\"", completePathBuf);
            return;
        }
    } else { // Create a symlink
        if (symlink(targetBuf, completePathBuf) == -1) {
            pid_status = MY_CUSTOM_EXIT_STATUS;
            perrorExtra("Could not create symlink with path \"%s\"", completePathBuf);
            return;
        }
    }
}

void m_status() {
    if (WIFEXITED(pid_status)) {
        printf("exited, m_status=%d\n", WEXITSTATUS(pid_status));
    } else if (WIFSIGNALED(pid_status)) {
        printf("killed by signal %d\n", WTERMSIG(pid_status));
    } else if (WIFSTOPPED(pid_status)) {
        printf("stopped by signal %d\n", WSTOPSIG(pid_status));
    } else if (WIFCONTINUED(pid_status)) {
        printf("continued\n");
    }
}
// ---------------------------------------- BUILT-IN COMMANDS ---------------------------------------- */


/* --------------------------------------- PROCESSES AND PIPES --------------------------------------- */

// Just a helper that executes the command and does stuff in case of an error
void execCommand(char *const *argv, const char *buf) {
    execvp(argv[0], argv);

    // Error
    free((char **) argv);
    perrorExtra("Couldn't execute command \"%s\"", buf);
    putchar('\n');
    exit(EXIT_FAILURE);
}

void executeExternal(unsigned argc, char *buf, const char *maxBuf) {
    char *const *argv = getArgv(argc, buf, maxBuf); // Get argv array
    if (argv == NULL) {
        // ERROR_PRINT
        doPrint(argc, buf, maxBuf, "argv == NULL\n");
        return;
    }

    if (isChild != 0) // Child code, comes from PIPE, already have a process for it, so we shouldn't create another one
    {
        execCommand(argv, buf);
    }

    pid_t pid, w;
    if ((pid = fork()) < 0) {
        free((char **) argv);
        perror("Couldn't start a process for exec external");
        return;
    }
    if (pid == 0) // Child code
    {
        execCommand(argv, buf);
    }
    w = waitpid(pid, &pid_status, WUNTRACED | WCONTINUED);
    if (w == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    free((char **) argv);
}

void executePipe(unsigned argc1, char *buf1, const char *maxBuf1,
                 unsigned argc2, char *buf2, const char *maxBuf2) {
    // TEST_PRINT
//    doPrint(argc1, buf1, maxBuf1, "command1");
//    doPrint(argc2, buf2, maxBuf2, "command2");

    int pfd1[2];
    pid_t pid, w;

    // CREATE PIPES
    // command1 | command2
    if (pipe(pfd1) < 0) { // command1 -> command2
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }
    // In parent:
    // command2 -> STDOUT (done automatically)

    // CREATE FIRST PROCESS (STDIN -> command1)
    if ((pid = fork()) < 0) {
        perror("Couldn't start a process for PIPE");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) { // Child code (STDIN -> command1)
        isChild = 1;

        // TEST_PRINT
//        doPrint(argc1, buf1, maxBuf1, "STDIN -> command1: ");

        // Close unused PIPEs
        close(pfd1[0]); // close the reading end of the pipe, child WRITES

        // Move output from command1
        dup2(pfd1[1], 1); // command1 -> PIPE1

        // Execute the command1
        parseCommand(argc1, buf1, maxBuf1);

        // Close the rest of the pipes
        close(pfd1[1]);

        // Done
        exit(0);
    }
    close(pfd1[1]); // close the WRITING end of the first pipe
    w = waitpid(pid, &pid_status, WUNTRACED | WCONTINUED); // And wait for it to finish
    if (w == -1) {
        perror("waitpid1");
        exit(EXIT_FAILURE);
    }

    // CREATE SECOND PROCESS (command1 -> command2)
    if ((pid = fork()) < 0) {
        perror("Couldn't start a process for PIPE");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) { // Child code (command1 -> command2)
        isChild = 1;

        // TEST_PRINT
//        doPrint(argc2, buf2, maxBuf2, "command1 -> command2: ");

        // Close unused PIPEs
        close(pfd1[1]); // I think it was closed in the parent already

        // Get input for command2
        dup2(pfd1[0], 0); // PIPE1 -> command2

        // Execute the command2
        parseCommand(argc2, buf2, maxBuf2);

        // Close the rest of the pipes
        close(pfd1[0]);

        // Done
        exit(0);
    }
    // Parent
    close(pfd1[0]); // close the READING end of the first pipe
    w = waitpid(pid, &pid_status, WUNTRACED | WCONTINUED);
    if (w == -1) {
        perror("waitpid2");
        exit(EXIT_FAILURE);
    }
}
// --------------------------------------- PROCESSES AND PIPES --------------------------------------- */


/* ----------------------------------------- SHELL SPECIFIC ------------------------------------------ */
void parseCommand(unsigned argc, char *buf, const char *maxBuf) {
    if (argc == 0)
        return;
    if (argc < 0 || buf == NULL || maxBuf == NULL) {
        // ERROR_PRINT
        doPrint(argc, buf, maxBuf, "parseCommand - early exit:\n");
        return;
    }

    // - I can use strcmp because I preprocessed the buf with '\0'

    // - canCallConditionedCommand() also helps with not repeating code, it just checks if I can execute the command

    // - For the commands that can't have other arguments (argc=1), I chose to still print the output,
    // while I do set the pid_status to an error.

    if (!strcmp(buf, "help")) { // Place this here as well to avoid going into executeExternal from "help"
        pid_status = 0;
        if (argc != 1) {
            fprintf(stderr, "help may not receive arguments!\n");
            pid_status = MY_CUSTOM_EXIT_STATUS;
        }
        m_help();
    }
    else if (!strcmp(buf, "exit")) {
        exit(0);
    }
    else if (!strcmp(buf, "cd")) {
        pid_status = 0;
        if (canCallConditionedCommand(argc == 2,
                                      "cd may only receive 1 argument!\n")) {
            m_chdir(buf);
        }
    }
    else if (!strcmp(buf, "pwd")) {
        pid_status = 0;
        if (argc != 1) {
            fprintf(stderr, "pwd may not receive arguments!\n");
            pid_status = MY_CUSTOM_EXIT_STATUS;
        }
        m_pwd();
    }
    else if (!strcmp(buf, "type")) {
        pid_status = 0;
        if (canCallConditionedCommand(argc == 2,
                                      "type may only receive 1 argument!\n")) {
            m_print_type(buf);
        }
    }
    else if (!strcmp(buf, "create")) {
        pid_status = 0;
        if (canCallConditionedCommand(3 <= argc && argc <= 5,
                                      "create may only receive 2/3/4 arguments!\n")) {
            m_create(argc, buf, maxBuf);
        }
    }
    else if (!strcmp(buf, "status")) {
        if (argc != 1)
            fprintf(stderr, "status may not receive arguments!\n");
        m_status();
        if (argc != 1)
            pid_status = MY_CUSTOM_EXIT_STATUS;
        else
            pid_status = 0;
    }
    else if (argc >= 1) {
        pid_status = 0;
        if (!strcmp(buf, "run"))
            buf += 4; // Jump over "run "
        executeExternal(argc, buf, maxBuf);
    }
}

_Noreturn void start_shell() {
    char buf[BUFF_SIZE + 1];
    char *cwd;
    ssize_t len;

    while (1) {
        fflush(stderr); // Had some issues with this one

        if ((cwd = getcwd(NULL, 0)) != NULL) {
            printf("%s> ", cwd);
            free(cwd);
        } else {
            printf("shell> ");
        }
        fflush(stdout);

        // TODO: Add a functionality to separate whole commands by '\n' (for bulk testing)
        if ((len = read(STDIN_FILENO, buf, BUFF_SIZE)) > 0) {
            buf[--len] = '\0';

            // These will be set in parseBuf
            unsigned argc1 = 0, argc2 = 0;
            char *buf1 = NULL, *buf2 = NULL;
            const char *maxBuf1 = NULL, *maxBuf2 = NULL;

            // Replaces every first ' ' it finds, with a '\0'
            // e.g. "abc    tip nume t_" ----> "abc_   tip_nume_t_"  // '_' is '\0'
            parseBuf(buf,
                     &argc1, &buf1, &maxBuf1,
                     &argc2, &buf2, &maxBuf2);

            if (argc2 > 0) { // This means we have arguments after the pipe as well
                executePipe(argc1, buf1, maxBuf1,
                            argc2, buf2, maxBuf2);
            } else {
                parseCommand(argc1, buf, maxBuf1);
            }
        }
    }
}
// ----------------------------------------- SHELL SPECIFIC ------------------------------------------ */

int main() {
    start_shell();
//    return 0;
}