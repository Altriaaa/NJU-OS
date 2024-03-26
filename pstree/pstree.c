#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>

//自定义一个pid结构体，用于存储pid, ppid, name, cpids
typedef struct
{
  pid_t pid;
  pid_t ppid;
  char* name;
  void* cpids;
} Pid;    

#define TEMPLATE_VECTOR(T) \
  typedef struct \
  { \
    int size; \
    int capacity; \
    T *data; \
  } Vector_##T; \
  Vector_##T *vector_##T##_create()   \
  { \
    Vector_##T *v = malloc(sizeof(Vector_##T));   \
    if(v == NULL)   \
    { \
      printf("malloc failed\n");                         \
      return NULL;     \
    }     \
    v->size = 0;     \
    v->capacity = 100;     \
    v->data = malloc(v->capacity * sizeof(T));     \
    if(v->data == NULL)     \
    {     \
      printf("malloc failed.\n");     \
      free(v);     \
    }     \
    return v;     \
  }     \
  void vector_##T##_destroy(Vector_##T* v)                        \
  {      \
    free(v->data);      \
    v->data = NULL;      \
    v->size = 0;      \
    v->capacity = 0;      \
    free(v);      \
  }             \
  int vector_##T##_push_back(Vector_##T* v, T p)             \
  {       \
    if(v->size == v->capacity)       \
    {       \
      v->capacity *= 2;       \
      T* tmp = realloc(v->data, v->capacity * sizeof(T)); \
      if(tmp == NULL)       \
      {       \
        printf("realloc failed.\n");       \
        return -1;       \
      }       \
      v->data = tmp;       \
    }       \
    v->data[v->size] = p;       \
    v->size++;       \
    return 0;       \
  }   

// typedef struct
// {
//   pid_t pid;
//   char* name;
// } pname;

TEMPLATE_VECTOR(Pid)    //声明一个Pid结构类型的Vector

//递归打印进程树
void print_tree(Vector_Pid* pids, int pid, int level, bool is_p)
{
  for(int i = 0; i < pids->size; i++)
  {
    if(pids->data[i].ppid == pid)
    {
      for(int j = 0; j < level; j++) printf("     ");
      if(is_p) printf("%s(%d)\n", pids->data[i].name, pids->data[i].pid);
      else printf("%s\n", pids->data[i].name);
      print_tree(pids, pids->data[i].pid, level + 1, is_p);
    }
  }
}

int main(int argc, char *argv[]) {

  //设置标志变量
  bool is_p = false;  //-p or --show-pids
  bool is_n = false;  //-n or --numeric-sort
  bool is_V = false;  //-V or --version

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    //printf("argv[%d] = %s\n", i, argv[i]);
    //读入参数，按照要求设置标志变量的值
    if (argv[i][0] == '-')
    {
      if(argv[i][1] == 'p' || strcmp(argv[i], "--show-pids") == 0)
        is_p = true;
      else if(argv[i][1] == 'n' || strcmp(argv[i], "--numeric-sort") == 0)
        is_n = true;
      else if(argv[i][1] == 'V' || strcmp(argv[i], "--version") == 0)
        is_V = true;
    }
  }

  //声明Vector
  Vector_Pid* pids = vector_Pid_create();

  //遍历/proc目录，寻找进程并获得它们的父进程
  struct dirent *de;          //指向目录项的指针
  DIR *dr = opendir("/proc"); //打开/proc目录
  if(dr == NULL)              //打开失败
  {
    printf("Could not open directory /proc\n");
    return -1;
  }
  while((de = readdir(dr)) != NULL)
  {
    Pid p = {0, 0, NULL, NULL};       //初始化pid结构体
    if(isdigit(de->d_name[0]))        //判断是否为数字开头的目录
    {
      char status_path[100];          //存储status文件的路径
      //构造status文件的路径
      strcpy(status_path, "/proc/");
      strcat(status_path, de->d_name);
      strcat(status_path, "/status");

      p.pid = atoi(de->d_name);        //将目录名转换为整数
      FILE *st = fopen(status_path, "r");
      if(st == NULL)                    //打开失败
      {
        printf("Could not open file %s\n", status_path);
        return -1;
      }
      char line[100];                   //存储每一行的内容
      while(fgets(line, sizeof(line), st))
      {
        if(strncmp(line, "Name:", 5) == 0) 
        {
          p.name = strdup(line + 6);    //将进程名存储到pid结构体中
          //去掉换行符
          for(int i = 0; i < strlen(p.name); i++)
          {
            if(p.name[i] == '\n')
            {
              p.name[i] = '\0';
              break;
            }
          }
        }
        if(strncmp(line, "PPid:", 5) == 0) 
        {
          p.ppid = atoi(line + 6);       //将父进程的pid转换为整数
          //printf("PID: %d, PPID: %d\n", pid, ppid);
          break;
        }
      }
      fclose(st);
    }
    if(p.name != NULL) vector_Pid_push_back(pids, p);  //将pid结构体存储到vector中
  }
  closedir(dr);

  //为每个进程寻找子进程
  for(int i = 0; i < pids->size; i++)
  {
    pids->data[i].cpids = vector_Pid_create();  //为每个进程创建一个cpids
    for(int j = 0; j < pids->size; j++)
    {
      if(pids->data[i].pid == pids->data[j].ppid)  //如果当前进程的pid等于另一个进程的ppid
      {
        vector_Pid_push_back((Vector_Pid*)(pids->data[i].cpids), pids->data[j]);  //将另一个进程存储到当前进程的cpids中
      }
    }
  }

  // //输出每个进程及其子进程
  // for(int i = 0; i < pids->size; i++)
  // {
  //   printf("%d %s\n", pids->data[i].pid, pids->data[i].name);
  //   for(int j = 0; j < ((Vector_Pid*)(pids->data[i].cpids))->size; j++)
  //   {
  //     printf("  %d %s\n", ((Vector_Pid*)(pids->data[i].cpids))->data[j].pid, ((Vector_Pid*)(pids->data[i].cpids))->data[j].name);
  //   }
  // }

  //根据参数选择输出
  if(is_V) //输出到标准错误输出
  {
    fprintf(stderr, "pstree (PSmisc) 23.6");
    return 0;
  }
  else
  {
    if(is_n) //若有-n参数，按照pid排序
    {
      for(int i = 0; i < pids->size; i++)
      {
        for(int j = i+1; j < pids->size; j++)
        {
          if(pids->data[i].pid > pids->data[j].pid)
          {
            Pid tmp = pids->data[i];
            pids->data[i] = pids->data[j];
            pids->data[j] = tmp;
          }
        }
      }
    }
  }

  //打印进程树
  print_tree(pids, 0, 0, is_p);


  assert(!argv[argc]);
  return 0;
}
