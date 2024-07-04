#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

typedef struct 
{
    char syscall_name[4096];
    double syscall_time;
} syscall_info;

syscall_info syscall_infos[1024];
int syscall_info_count = 0;
double total_time = 0;

void add_new(char* syscall_name, double syscall_time)
{
    for(int i = 0; i < syscall_info_count; i++)
    {
        if(strcmp(syscall_name, syscall_infos[i].syscall_name) == 0)
        {
            syscall_infos[i].syscall_time += syscall_time;
            total_time += syscall_time;
            return;
        }
    }
    strcpy(syscall_infos[syscall_info_count].syscall_name, syscall_name);
    syscall_infos[syscall_info_count].syscall_time = syscall_time;
    syscall_info_count++;
    total_time += syscall_time;
}

void print_top5()
{
    for(int i = 0; i < syscall_info_count; i++)
    {
        for(int j = i+1; j < syscall_info_count; j++)
        {
            if(syscall_infos[i].syscall_time < syscall_infos[j].syscall_time)
            {
                syscall_info tmp = syscall_infos[i];
                syscall_infos[i] = syscall_infos[j];
                syscall_infos[j] = tmp;
            }
        }
    }
    for(int i = 0; i < syscall_info_count && i < 5; i++)
    {
        printf("%s (%d%%)\n", syscall_infos[i].syscall_name, (int)(syscall_infos[i].syscall_time/total_time*100));
    }
    for(int i = 0; i < 80; i++)
    {
        putchar('\0');
    }
    fflush(stdout);
}

int main(int argc, char *argv[], char *envp[]) 
{
    int pipefd[2];
    if(pipe(pipefd) == -1)
    {
        perror("pipe");
        return 1;
    }
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork");
        return 1;
    }
    if(pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        char* exec_argv[4096];
        exec_argv[0] = "strace";
        exec_argv[1] = "-T";
        exec_argv[2] = "-ttt";
        for(int i = 3; i <= argc+1; i++)
        {
            exec_argv[i] = argv[i-2];
        }
        exec_argv[argc+2] = NULL;
        execve("/usr/bin/strace", exec_argv, envp);
        perror("execve");
        return 1;
    }
    else
    {
        close(pipefd[1]);
        char buffer[4096];
        char line[4096];
        ssize_t n;
        char* start = buffer;
        char* end;
        int line_over = 1;
        char syscall_name[2048];
        double last_print_time = 0;
        while((n = read(pipefd[0], buffer, sizeof(buffer)-1)) > 0)
        {
            buffer[n] = '\0';
            start = buffer;
            while ((end = strchr(start, '\n')) != NULL)
            {
                *end = '\0';
                if(line_over)
                {
                    strncpy(line, start, sizeof(line)-1);
                    line[sizeof(line)-1] = '\0';
                }
                else
                {
                    strncat(line, start, sizeof(line)-strlen(line)-1);
                    line[sizeof(line)-1] = '\0';
                    line_over = 1;
                }
                start = end + 1;

                // process in one line
                char* s = strchr(line, ' ');
                if(s != NULL)
                {
                    *s = '\0';
                    double cur_time = atof(line);
                    if(cur_time - last_print_time > 0.1)
                    {
                        print_top5();
                        last_print_time = cur_time;
                    }
                    s = s + 1;
                }
                else
                {
                    s = line;
                }
                char* p = strchr(s, '(');
                if(p != NULL)
                {
                    *p = '\0';
                    strncpy(syscall_name, s, sizeof(syscall_name)-1);
                    syscall_name[sizeof(syscall_name)-1] = '\0';
                    *p = '(';
                    // printf("%s     ", syscall_name);
                    // fflush(stdout);
                }

                char syscall_time[2048];
                char* l = NULL;
                for(int i = strlen(s)-1; i >= 0; i--)
                {
                    if(s[i] == '<')
                    {
                        l = s + i + 1;
                        break;
                    }
                }
                if(l == NULL)
                {
                    continue;
                }
                char* r = NULL;
                for(int i = 0; i < strlen(l); i++)
                {
                    if(l[i] == '>')
                    {
                        r = l + i;
                        break;
                    }
                }
                if(r == NULL)
                {
                    continue;
                }
                strncpy(syscall_time, l, r-l);
                syscall_time[r-l] = '\0';
                double time = atof(syscall_time);
                add_new(syscall_name, time);
                // printf("%s\n", syscall_time);
            }
            if(strchr(start, '\n') == NULL)
            {
                if(line_over == 1)
                {
                    strncpy(line, start, sizeof(line)-1);
                    line[sizeof(line)-1] = '\0';
                    line_over = 0;
                }
                else
                {
                    strncat(line, start, sizeof(line)-strlen(line)-1);
                    line[sizeof(line)-1] = '\0';
                }
            }
        }

        close(pipefd[0]);
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status))
        {
            print_top5();
            return WEXITSTATUS(status);
        }
    }
    return 0;
}
