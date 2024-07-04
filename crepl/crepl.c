#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>

int expr_count = 0;
const char* lib_dir = "/tmp";
char c_filename[4100];

void handle_func(char* line)
{
    // Create a file in /tmp
    char filename[2048];
    snprintf(filename, sizeof(filename), "/tmp/funcXXXXXX");
    int fd_ = mkstemp(filename);
    if (fd_ == -1) 
    {
        perror("mkstemp");
        return;
    }
    // Rename the file with .c extension
    char func_filename[4096];
    snprintf(func_filename, sizeof(func_filename), "%s.c", filename);
    if (rename(filename, func_filename) == -1) 
    {
        perror("rename");
        return;
    }
    // compile the file
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork");
        return;
    }
    if(pid == 0)
    {
        execlp("gcc", "gcc", "-fPIC", "-shared", "-o", "/tmp/func.so", func_filename, NULL);
        perror("execlp");
        return;
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            // write line to the file
            int fd = open(c_filename, O_RDWR);
            FILE* file = fdopen(fd, "a");
            if(file == NULL)
            {
                perror("fdopen");
                return;
            }
            fprintf(file, "%s", line);
            fclose(file);
            printf("success\n");
            fflush(stdout);
        }
        else
        {
            printf("fail\n");
            fflush(stdout);
        }
    }
    // delete the temporary file
    unlink(func_filename);
    unlink("/tmp/func.so");
    return;
}   

void handle_expr(char* line)
{
    // make a wrapper function
    char wrapper[3000];
    snprintf(wrapper, sizeof(wrapper), "int expr_%d() { return %s; }", expr_count, line);
    // Create a file in /tmp
    char filename[2048];
    snprintf(filename, sizeof(filename), "/tmp/exprXXXXXX");
    int fd_ = mkstemp(filename);
    if (fd_ == -1) 
    {
        perror("mkstemp");
        return;
    }
    // Rename the file with .c extension
    char expr_filename[4096];
    snprintf(expr_filename, sizeof(expr_filename), "%s.c", filename);
    if (rename(filename, expr_filename) == -1) 
    {
        perror("rename");
        return;
    }
    // write wrapper to the file
    int fd = open(expr_filename, O_RDWR);
    FILE* file = fdopen(fd, "a");
    if(file == NULL)
    {
        perror("fdopen");
        return;
    }
    fprintf(file, "#include \"%s\"\n", c_filename);
    fprintf(file, "%s", wrapper);
    fclose(file);
    // compile the file
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork");
        return;
    }
    if(pid == 0)
    {
        execlp("gcc", "gcc", "-fPIC", "-shared", "-o", "/tmp/expr.so", expr_filename, NULL);
        perror("execlp");
        return;
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fflush(stdout);
        }
        else
        {
            printf("fail\n");
            fflush(stdout);
            return;
        }
    }
    pid_t pid3 = fork();
    if(pid3 < 0)
    {
        perror("fork");
        return;
    }
    if(pid3 == 0)
    {
        execlp("gcc", "gcc", "-m32","-fPIC", "-shared",  "-o", "/tmp/expr_32.so", expr_filename, NULL);
        perror("execlp");
        return;
    }
    else
    {
        int status;
        waitpid(pid3, &status, 0);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fflush(stdout);
        }
        else
        {
            printf("fail\n");
            fflush(stdout);
            return;
        }
    }
    // load the shared object
    pid_t pid2 = fork();
    if(pid2 < 0)
    {
        perror("fork");
        return;
    }
    if(pid2 == 0)
    {
        void* handle = dlopen("/tmp/expr.so", RTLD_LAZY);
        if(handle == NULL)
        {
            // printf("dlopen: %s\n", dlerror());
            // return;
            handle = dlopen("/tmp/expr_32.so", RTLD_LAZY);
            if(handle == NULL)
            {
                printf("dlopen: %s\n", dlerror());
                fflush(stdout);
                return;
            }
        }
        dlerror();
        // get the function pointer
        char func_name[100];
        snprintf(func_name, sizeof(func_name), "expr_%d", expr_count);
        int (*func)() = dlsym(handle, func_name);
        char* error = dlerror();
        if(error != NULL)
        {
            fprintf(stderr, "%s\n", error);
            dlclose(handle);
            return;
        }
        // call the function
        int result = func();
        printf("%d\n", result);
        fflush(stdout);
        // close the shared object
        dlclose(handle);
        return;
    }
    else
    {
        int status;
        waitpid(pid2, &status, 0);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fflush(stdout);
        }
        else
        {
            printf("fail\n");
            fflush(stdout);
        }
    }
    // delete the temporary file
    unlink(expr_filename);
    unlink("/tmp/expr.so");
    return;
}

int main(int argc, char *argv[]) 
{
    static char line[4096];

    // Create the file in /tmp
    char filename[4096];
    snprintf(filename, sizeof(filename), "/tmp/creplXXXXXX");
    int fd = mkstemp(filename);
    if (fd == -1) 
    {
        perror("mkstemp");
        return 1;
    }
    // Rename the file with .c extension   
    snprintf(c_filename, sizeof(c_filename), "%s.c", filename);
    if (rename(filename, c_filename) == -1) 
    {
        perror("rename");
        return 1;
    }


    while (1) {
        printf("crepl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        // if the first 3 characters are "int", handle_func(line);
        if(strncmp(line, "int", 3) == 0) 
        {
            handle_func(line);
        } 
        else 
        {
            expr_count++;
            handle_expr(line);
        }
    }
}
