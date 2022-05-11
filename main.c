#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFF_SIZE 10000

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

void parseCommand(unsigned argc, char *buf, const char *maxBuf);

void help() {
    // TODO: Update with new stuff
    printf("This is a shell that can do the following:\n"
           "1. A promt functionality\n"
           "2. Execute commands syncronously\n"
           "3. Built-in commands:\n"
           "   - help         -> displays this message\n"
           "   - exit         -> exits the shell\n"
           "   - cd <DIR>     -> changes the current directory\n"
           "   - pwd          -> prints the current directory\n"
           "   - type <ENTRY> -> displays the type of ENTRY (absolute/relative path)\n"
           "   - create <TYPE> <NAME> [TARGET] [DIR]\n"
           "      - creates a new entry of the given TYPE, where TYPE may have one of the following values:\n"
           "            -> -f  (regular file) -> no TARGET or DIR\n"
           "            -> -l  (symlink) -> <TARGET>, [DIR]{.}\n"
           "            -> -d  (directory) -> no TARGET or DIR\n"
           "4. run UNIX commands:\n"
           "   - <COMMAND> [ARG1 ARG2 … ] will execute the given COMMAND with an arbitrary number of arguments\n"
           "   - status -> displays the exit status of the previously executed command\n"
           "5. Support for connecting two commands using one pipe (e.g. ls -l -a | sort)\n"
           "\n");
}

const char *jumpWhitespaces(const char *buf) {
    while (isspace(*buf)) // jump over a whitespace
        ++buf;

    return buf;
}

// "create    tip nume t     d_"
// "create_   tip_nume_t_    d_"  // _ is \0
void parseBuf(char *buf,
              unsigned *argc1, char **buf1, const char **maxBuf1,
              unsigned *argc2, char **buf2, const char **maxBuf2) {
    // TODO: "ls -a|sort" -> should work such a case
    *argc1 = 0;
    *argc2 = 0;
    unsigned *argc = argc1; // Start with argc1

    buf = (char *) jumpWhitespaces(buf); // Jump if there are whitespaces at the start of the buffer
    *buf1 = buf; // The first buffer starts after those jumped whitespaces

    char *last0 = NULL, *lastChar = NULL;
    char hadSpace = 1, hadPipe = 0;
    while (*buf) {
        if (*buf == '|') {
            if (*(buf + 1) == '\0') { // If it ends with a pipe, we should just remove it
                *buf = '\0';
            }

            hadSpace = 1; // Consider '|' as space as well
            hadPipe = 1;
            *maxBuf1 = last0; // Set the end of the first one to be at the last 0 placed

            buf = (char *) jumpWhitespaces(buf + 1); // Jump over the possible starting whitespaces
            *buf2 = buf; // The second buffer starts after those jumped whitespaces

            argc = argc2; // Switched argc
        } else if (isspace(*buf)) {
            hadSpace = 1;
            last0 = buf;

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

    if(*maxBuf1 == NULL) {
        *maxBuf1 = lastChar + 1; // including the last '\0'
    }
    else if(hadPipe){
        if(hadSpace) {
            *maxBuf2 = last0; // including the last '\0'
        } else{
            *maxBuf2 = lastChar + 1; // including the last '\0'
        }
    }

//    putchar('\n');
//    putchar('\n');
//    putchar('_');
//    for (buf = *buf1; buf < *maxBuf1; ++buf) {
//        putchar(*buf);
//    }
//    putchar('_');
//    putchar('\n');
//    putchar('\n');
}

// "create_   tip_nume_t_    d_"
// "tip_nume_t_    d_"
// FAILED when return points towards 0
const char *getNextArg(const char *buf, const char *maxBuf) {
    for (; *buf; ++buf);
    if (buf == maxBuf)
        return buf; // The last one, nothing afterwards, end with '\0'
    if (buf > maxBuf) {
        printf("buf > maxBuf; maxBuf is not calculated correctly!!\n");
        exit(-1);
    }

    return jumpWhitespaces(buf + 1);
}

char *const *getArgv(unsigned argc, const char *buf, const char *maxBuf) {
    if (argc == 0)
        return NULL;

    const char **argv = malloc(sizeof(char *) * (argc + 1));
    if(argv == NULL){
        perror("getArgv - Couldn't allocate memory!!\n");
        help();
        exit(-1);
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

void m_chdir(char *buf) {
    if (chdir(jumpWhitespaces(buf + 3)) == -1) {
        perror("ERROR cd");
        help();
    }
}

void m_pwd() {
    char *cwd;
    if ((cwd = getcwd(NULL, 0)) != NULL)
        printf("%s\n", cwd);
    else {
        perror("ERROR pwd");
        help();
    }
}

int m_get_type(const char *buf) {
    // TODO: This doesn't work for symlinks (says regular file)
    struct stat sb;
    if (stat(jumpWhitespaces(buf + 5), &sb) == -1) {
        perror("ERROR type");
        help();
        return -1;
    }
    return sb.st_mode & S_IFMT;
}

void m_print_type(char *buf) {
    int type = m_get_type(buf);
    if(type != -1) {
        switch (type) {
            case S_IFBLK:  printf("block device\n");            break;
            case S_IFCHR:  printf("character device\n");        break;
            case S_IFDIR:  printf("directory\n");               break;
            case S_IFIFO:  printf("FIFO/pipe\n");               break;
            case S_IFLNK:  printf("symlink\n");                 break;
            case S_IFREG:  printf("regular file\n");            break;
            case S_IFSOCK: printf("socket\n");                  break;
            default:       printf("unknown?\n");                break;
        }
    }
}

FILE *fout_TEST = NULL;

void checkOpenFile(){
    if(fout_TEST == NULL) { // Not yet opened (pointer copied to processes, so it's okay)
        fout_TEST = fopen("shell.log", "w");

        if(fout_TEST == NULL) {
            perror("checkOpenFile");
            exit(-1);
        }
    }
}

void doPrint(unsigned argc, char *buf, const char *maxBuf, const char* stringToPrint) {
    checkOpenFile();

    fprintf(fout_TEST, "%s", stringToPrint);

    fprintf(fout_TEST, "argc=%u; ", argc);
    if(argc > 0) {
        char *const *argv = getArgv(argc, buf, maxBuf);
        fprintf(fout_TEST, "argv= ");
        for (; *argv != NULL; ++argv)
            fprintf(fout_TEST, "_%s_", *argv);
    }
    if(maxBuf > buf)
        fprintf(fout_TEST, " {[%p, %p], [%c, %c]}", buf, maxBuf, *buf, *(maxBuf - 1));
    else
        fprintf(fout_TEST, " {[%p, %p], buf < maxBuf}", buf, maxBuf);
    putc('\n', fout_TEST);

    fflush(fout_TEST);
}

int supportedFileType(const char *buf) {
    return !strcmp(buf, "-f") || !strcmp(buf, "-l") || !strcmp(buf, "-d");
}

void m_create(char *buf, const char *maxBuf) {
    // TODO: When -l, argc=[4, 5], with dir being optional, otherwise, argc=[1, 3]
    char *typeBuf = (char *) jumpWhitespaces(buf + 7);
    char *nameBuf = (char *) getNextArg(typeBuf, maxBuf);
    if (!(*typeBuf) || !(*nameBuf)) {
        printf("No type/name provided\n\n");
        help();
        return;
    }
    if (!supportedFileType(typeBuf)) {
        printf("File type not supported\n\n");
        help();
        return;
    }

    char *targetBuf = (char *) getNextArg(nameBuf, maxBuf);

    // Prepare the directory // TODO: ADD THIS TO -f and -d as well !!!!!!!!!
    char dirBuf[2001]; // TODO: When this doesn't exist, do something special maybe?
    strcpy(dirBuf, getNextArg(targetBuf, maxBuf));
    if (!(*dirBuf)) { // Get the current directory if DIR was not provided
        char *cwd;
        if ((cwd = getcwd(NULL, 0)) != NULL) {
            strcpy(dirBuf, cwd);
        } else {
            perror("ERROR with getting current directory");
        }
    }

    // Add the filename to the directory
    strcat(dirBuf, "/");
    strcat(dirBuf, nameBuf);

    // TEST_PRINT
//    checkOpenFile();
//    fprintf(fout_TEST, "TYPE  : |%s|\n", typeBuf);
//    fprintf(fout_TEST, "NAME  : |%s|\n", nameBuf);
//    fprintf(fout_TEST, "TARGET: |%s|\n", targetBuf);
//    fprintf(fout_TEST, "DIR   : |%s|\n\n", dirBuf);

    if (!strcmp(typeBuf, "-f")) {
        int fp;
        if ((fp = open(dirBuf, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH)) == -1) {
            perror("Could not create file");
            help();
            return;
        }
        if (close(fp) == -1) {
            perror("Could not close file");
            help();
            return;
        }
    } else if (!strcmp(typeBuf, "-d")) {
        if (mkdir(dirBuf, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
            perror("Could not create directory");
            help();
            return;
        }
    } else { // -l
        if (symlink(targetBuf, dirBuf) == -1) {
            perror("Could not create a symlink");
            help();
            return;
        }
    }
}

int pid_status;
int isChild;

void execCommand(char *const *argv) {
    execvp(argv[0], argv);

    // Error
    free((char **) argv);
    perror("Couldn't start a process for exec");
    putchar('\n');
    help();
    exit(-1);
}

void executeExternal(unsigned argc, char *buf, const char *maxBuf) {
    // TODO: Connect stderr of processes
    char *const *argv = getArgv(argc, buf, maxBuf);
    if (argv == NULL) {
        // ERROR_PRINT
        doPrint(argc, buf, maxBuf, "argv == NULL\n");
        return;
    }

    // TEST_PRINT
//    checkOpenFile();
//    fprintf(fout_TEST, "isChild INSIDE=%u\n", isChild);
//    for (unsigned i = 0; i < argc; i++)
//        fprintf(fout_TEST, "_%s\n", argv[i]);
//    fprintf(fout_TEST, "\n");
//    fflush(fout_TEST);

    if (isChild != 0) // Child code, comes from PIPE, already have different process for it
    {
        execCommand(argv);
    }

    pid_t pid, w;
    if ((pid = fork()) < 0) {
        free((char **) argv);
        perror("Couldn't start a process for exec external");
        help();
        return;
    }
    if (pid == 0) // Child code
    {
        execCommand(argv);
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
    // TODO: Connect stderr of processes
    // TODO: "ls a | sort" -> when executing this command, the command "ls a" is executed first, then "ls"
    int pfd1[2];
    int pfd2[2];
    pid_t pid, w;

    // CREATE PIPES
    // command1 | command2
    if (pipe(pfd1) < 0) { // STDIN -> command1
        perror("Pipe creation failed");
        exit(-1);
    }
    if (pipe(pfd2) < 0) { // command1 -> command2
        perror("Pipe creation failed");
        exit(-1);
    }
    // In parent:
    // command2 -> STDOUT

    // CREATE FIRST PROCESS (STDIN -> command1)
    if ((pid = fork()) < 0) {
        perror("Couldn't start a process for PIPE");
        help();
        exit(-1);
    }
    if (pid == 0) { // Child code (STDIN -> command1)
        isChild = 1;

        // TEST_PRINT
//        doPrint(argc1, buf1, maxBuf1, "STDIN -> command1: ");

        // Close unused PIPEs
        close(pfd1[0]); // close the reading end of the pipe, child WRITES
        close(pfd2[0]);
        close(pfd2[1]);

        // Execute the command1
        dup2(pfd1[1], 1); // command1 -> PIPE1
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
        help();
        exit(-1);
    }
    if (pid == 0) { // Child code (command1 -> command2)
        isChild = 1;

        // TEST_PRINT
//        doPrint(argc2, buf2, maxBuf2, "command1 -> command2: ");

        // Close unused PIPEs
        close(pfd2[0]); // close the reading end of the pipe, child WRITES
        close(pfd1[1]); // I think it was closed in the parent already

        // Get input for command2
        dup2(pfd1[0], 0); // PIPE1 -> command2

        // Execute the command2
        dup2(pfd2[1], 1); // command2 -> PIPE2
        parseCommand(argc2, buf2, maxBuf2);

        // Close the rest of the pipes
        close(pfd2[1]);
        close(pfd1[0]);

        // Done
        exit(0);
    }
    // Parent
    close(pfd1[0]); // close the READING end of the first pipe
    close(pfd2[1]); // close the WRITING end of the second pipe
    w = waitpid(pid, &pid_status, WUNTRACED | WCONTINUED);
    if (w == -1) {
        perror("waitpid2");
        exit(EXIT_FAILURE);
    }

    char concat_str[BUFF_SIZE];
    read(pfd2[0], concat_str, BUFF_SIZE);
    printf("%s", concat_str);

    close(pfd2[0]); // close the reading end of the pipe as well
}

void status() {
    // TODO: Should print correctly the last status for builtin commands as well
    if (WIFEXITED(pid_status)) {
        printf("exited, status=%d\n", WEXITSTATUS(pid_status));
    } else if (WIFSIGNALED(pid_status)) {
        printf("killed by signal %d\n", WTERMSIG(pid_status));
    } else if (WIFSTOPPED(pid_status)) {
        printf("stopped by signal %d\n", WSTOPSIG(pid_status));
    } else if (WIFCONTINUED(pid_status)) {
        printf("continued\n");
    }
}

void parseCommand(unsigned argc, char *buf, const char *maxBuf) {
    if(argc == 0)
        return;
    if(argc < 0 || buf == NULL || maxBuf == NULL) {
        // ERROR_PRINT
        doPrint(argc, buf, maxBuf, "parseCommand - early exit:\n");
        return;
    }

    pid_status = 0; // TODO: Check if this is correct
    if (argc == 1 && !strcmp(buf, "help")) { // Place this here as well to avoid going into executeExternal from "help"
        help();
    } else if (!strcmp(buf, "exit")) {
        exit(0);
    } else if (argc == 2 && !strcmp(buf, "cd")) { // I can use strcmp because I preprocessed the buf with '\0'
        m_chdir(buf);
    } else if (!strcmp(buf, "pwd")) {
        if (argc != 1) {
            printf("pwd may not receive arguments!\n");
            help();
        } else {
            m_pwd();
        }
    } else if (!strcmp(buf, "type")) {
        if (argc != 2) {
            printf("type may only receive 1 argument!\n");
            help();
        } else {
            m_print_type(buf);
        }
    } else if (!strcmp(buf, "create")) {
        if (argc != 3 && argc != 5) {
            printf("create may only receive 3 or 5 arguments!\n");
            help();
        } else {
            m_create(buf, maxBuf);
        }
    } else if (!strcmp(buf, "status")) {
        if (argc != 1) {
            printf("status may not receive arguments!\n");
            help();
        } else {
            status();
        }
    } else if (argc >= 1) {
        executeExternal(argc, buf, maxBuf);
    }
}

_Noreturn void start_shell() {
    char buf[BUFF_SIZE];
    char *cwd;
    ssize_t len;

    while (1) {
        // TODO: Flush stdout, stderr before start of shell ???
//        fflush(stderr); // Had some issues with this one
//        fflush(stdout);

        if ((cwd = getcwd(NULL, 0)) != NULL)
            printf("%s > ", cwd);
        else
            printf("shell > ");
        fflush(stdout);

        if ((len = read(STDIN_FILENO, buf, BUFF_SIZE)) > 0) {
            buf[--len] = 0;

            unsigned argc1 = 0, argc2 = 0;
            char *buf1 = NULL, *buf2 = NULL;
            const char *maxBuf1 = NULL, *maxBuf2 = NULL;

            // e.g. "abc    tip nume t_" -> "abc_   tip_nume_t_"  // '_' is '\0'
            parseBuf(buf,
                     &argc1, &buf1, &maxBuf1,
                     &argc2, &buf2, &maxBuf2);

            if(argc2 > 0) { // This means we have arguments after the pipe as well
                executePipe(argc1, buf1, maxBuf1,
                            argc2, buf2, maxBuf2);
            }
            else {
                parseCommand(argc1, buf, maxBuf1);
            }
        }
    }
}

int main() {
    start_shell();
//    return 0;
}