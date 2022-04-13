#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

// For Windows
#if (defined(_WIN32) || defined(__WIN32__))
#define mkdir(A, B) mkdir(A)

#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif

#endif

#if (defined(_WIN32) || defined(__WIN32__))
#include <windows.h>
#include <winbase.h>

int m_get_type(const char *buf);

int symlink(const char *target, const char *linkpath) {
    int type = m_get_type(target);
    if (type == S_IFDIR) { // Target is a directory
        // FAIL
        if (!CreateSymbolicLinkA(linkpath, target, SYMBOLIC_LINK_FLAG_DIRECTORY))
            return -1;
    } else if (type == S_IFREG) { // Target is a file
        // FAIL
        if (!CreateSymbolicLinkA(linkpath, target, 0))
            return -1;
    } else
        return -1; // target not known
    return 0;
}
#endif
// End for Windows

// for cd       -> (use chdir)
// for pwd      -> (use getcwd())
// for type     -> ()
// for create   -> ()

void help() {
    printf("This is a shell that can do the following:\n"
           "1. A promt functionality\n"
           "2. Execute commands syncronously\n"
           "3. Built-in commands:\n"
           "   - help\n"
           "   - exit\n"
           "   - cd\n"
           "   - pwd\n"
           "   - type ENTRY\n"
           "   - create TYPE NAME [TARGET] [DIR]\n"
           "            -> -f  (regular file)\n"
           "            -> -l  (symlink)\n"
           "            -> -d  (directory)\n"
           "\n");
}

const char *jumpWhitespaces(const char *buf) {
    while (isspace(*buf)) // jump over a whitespace
        ++buf;

    return buf;
}

// "create    tip nume t     d_"
// "create_   tip_nume_t_    d_"  // _ is \0
unsigned processBuf(char *buf) {
    unsigned argc=1;
    buf = (char *)jumpWhitespaces(buf); // Jump if there are whitespaces at the start of the buffer

    for (; *buf; ++buf) {
        if (isspace(*buf)) {
            *buf = 0;
            buf = (char *)jumpWhitespaces(buf + 1); // Jump over the rest of the whitespaces
            ++argc;
        }
    }

    return argc;
}

// "create_   tip_nume_t_    d_"
// "tip_nume_t_    d_"
const char *getNextArg(char *buf, const char *maxBuf){
    for (; *buf; ++buf);
    if(buf >= maxBuf)
        return buf;

    return jumpWhitespaces(buf + 1);
}

void m_chdir(char *buf){
    if(chdir(jumpWhitespaces(buf + 3)) == -1) {
        perror("ERROR cd");
        help();
    }
}

void m_pwd(){
    char *cwd;
    if ((cwd = getcwd(NULL, 0)) != NULL)
        printf("%s\n", cwd);
    else {
        perror("ERROR pwd");
        help();
    }
}

int m_get_type(const char *buf){
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

int supportedFileType(const char *buf){
    return !strcmp(buf, "-f") || !strcmp(buf, "-l") || !strcmp(buf, "-d");
}

void m_create(char *buf, const char *maxBuf) {
    char *typeBuf = (char *)jumpWhitespaces(buf + 7);
    char *nameBuf = (char *)getNextArg(typeBuf, maxBuf);
    if (!(*typeBuf) || !(*nameBuf)) {
        printf("No type/name provided\n");
        help();
        return;
    }
    if (!supportedFileType(typeBuf)) {
        printf("File type not supported\n");
        help();
        return;
    }

    char *targetBuf = (char *)getNextArg(nameBuf, maxBuf);

    // Prepare the directory
    char dirBuf[2001];
    strcpy(dirBuf, getNextArg(targetBuf, maxBuf));
    if (!(*dirBuf)) {
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

    // Some prints
    printf("TYPE  : |%s|\n", typeBuf);
    printf("NAME  : |%s|\n", nameBuf);
    printf("TARGET: |%s|\n", targetBuf);
    printf("DIR   : |%s|\n\n", dirBuf);

    if (!strcmp(typeBuf, "-f")) {
        int fp;
        if ((fp = open(dirBuf, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH)) == -1) {
            perror("Could not create file");
            help();
            return;
        }
        if(close(fp) == -1){
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

void start_shell(){
    char buf[1001];
    ssize_t len;

    printf("shell > ");
    fflush(stdout);
    while ((len = read(STDIN_FILENO, buf, 1000)) > 0) {
        buf[--len] = 0;
        printf("len   : |%d| ", len);
        printf("buffer: |%s|\n", buf);

        const char *maxBuf = buf + len; // including the last '\0'

        unsigned argc = processBuf(buf);    // "create    tip nume t     d_"
                                            // "create_   tip_nume_t_    d_"  // '_' is '\0' actually
//        printf("proc  : |%s|\n", buf);
//        printf("argc  : |%u|\n", argc);

        if (!strcmp(buf, "exit")) {
            exit(0);
        } else if (argc == 2 && !strcmp(buf, "cd")) { // I can use strcmp because I preprocessed the buf with '\0'
            m_chdir(buf);
        } else if (!strcmp(buf, "pwd")) {
            if(argc != 1){
                printf("pwd may not receive arguments\n");
                help();
            } else {
                m_pwd();
            }
        } else if (!strcmp(buf, "type")) {
            if(argc != 2){
                printf("type may only receive 1 argument\n");
                help();
            } else {
                m_print_type(buf);
            }
        } else if (!strcmp(buf, "create")) {
            if(argc != 3 && argc != 5){
                printf("create may only receive 3 or 5 arguments\n");
                help();
            } else {
                m_create(buf, maxBuf);
            }
        } else {
            help();
        }

        printf("shell > ");
        fflush(stdout);
    }
}

int main() {
    start_shell();
    return 0;
}