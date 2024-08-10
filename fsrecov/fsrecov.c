#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "fat32.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define ceil(a, b) ((a) % (b) == 0 ? (a) / (b) : (a) / (b) + 1)
#define MAX_CLUS_NUM 32768
#define CLUS_SIZE hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus
#define PAD_CHK_TIMES 1

struct fat32hdr *hdr;

void *mmap_disk(const char *fname);
void scan();

int DirClus[MAX_CLUS_NUM] = {0};
int BMPClus[MAX_CLUS_NUM] = {0};
int BMPdataClus[MAX_CLUS_NUM] = {0};
int DirClusNum = 0;
int BMPClusNum = 0;
int BMPdataClusNum = 0;

int main(int argc, char *argv[]) 
{
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
        exit(1);
    }
    setbuf(stdout, NULL);

    assert(sizeof(struct fat32hdr) == 512);
    assert(sizeof(fat32dent) == 32);

    hdr = mmap_disk(argv[1]);
    scan();
    munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}

void *mmap_disk(const char *fname) 
{
    int fd = open(fname, O_RDWR);

    if (fd < 0) {
        goto release;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        goto release;
    }

    struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (hdr == MAP_FAILED) {
        goto release;
    }

    close(fd);

    assert(hdr->Signature_word == 0xaa55); // this is an MBR
    assert(hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec == size);

    printf("%s: DOS/MBR boot sector, ", fname);
    printf("OEM-ID \"%s\", ", hdr->BS_OEMName);
    printf("sectors/cluster %d, ", hdr->BPB_SecPerClus);
    printf("sectors %d, ", hdr->BPB_TotSec32);
    printf("sectors/FAT %d, ", hdr->BPB_FATSz32);
    printf("serial number 0x%x\n", hdr->BS_VolID);
    return hdr;

release:
    perror("map disk");
    if (fd > 0) {
        close(fd);
    }
    exit(1);
}

void *cluster_to_sec(int n) 
{
    u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
    DataSec += (n - 2) * hdr->BPB_SecPerClus;
    return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

void get_filename(fat32dent *dent, char *buf) 
{
    int len = 0;
    for (int i = 0; i < sizeof(dent->sdir.DIR_Name); i++) {
        if (dent->sdir.DIR_Name[i] != ' ') {
            if (i == 8)
                buf[len++] = '.';
            buf[len++] = dent->sdir.DIR_Name[i];
        }
    }
    buf[len] = '\0';
}

int is_DirClus(u32 clusID)  //通过目录的某些特征，判断该簇是否为目录簇
{
    int ndents = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(fat32dent);
    for (int d = 0; d < ndents;)
    {
        fat32dent* dent = (fat32dent*)cluster_to_sec(clusID) + d;
        if(d == 0 && dent->sdir.DIR_Name[0] == 0x00) return 0; //not used
        if(dent->sdir.DIR_Name[0] == 0x00) break;    //该簇中，所有在此目录项之后的目录项都为空
        else if(dent->sdir.DIR_Name[0] == 0xe5) d++;    //该目录项为空
        else if(dent->ldir.LDIR_Attr == ATTR_LONG_NAME) //该目录项为长文件名
        {
            //首先检查长文件名是否合法：LDIR_Type == 0x00 && LDIRFstClusLO == 0x00
            if(dent->ldir.LDIR_Type != 0x00 || dent->ldir.LDIR_FstClusLO != 0x00) return 0;
            //确定长文件名的长度
            int len = dent->ldir.LDIR_Ord;
            if(len > LAST_LONG_ENTRY) len ^= LAST_LONG_ENTRY; //清除0x40标志位
            len = min(len, ndents -1 -d); //防止越界
            for(int i = 1; i < len; i++)
            {
                fat32dent* nxt_dent = (fat32dent*)cluster_to_sec(clusID) + d + i;
                if(nxt_dent->ldir.LDIR_Ord != len - i || nxt_dent->ldir.LDIR_Type != 0x00 || nxt_dent->ldir.LDIR_FstClusLO != 0x00) return 0;
            }
            // check the following normal file name
            fat32dent* nxt_dent = (fat32dent*)cluster_to_sec(clusID) + d + len;
            if(nxt_dent->sdir.DIR_Name[0] == 0x00 || nxt_dent->sdir.DIR_NTRes != 0) return 0;
            d += len + 1;
        }
        else if(dent->sdir.DIR_Name[0] != 0x00) //普通的目录项
        {
            if(dent->sdir.DIR_NTRes != 0 || dent->sdir.DIR_Attr != ATTR_DIRECTORY) return 0;
            d++;
        }
    }
    return 1;
}

int is_BMPhdr(u32 clusID)
{
    BMP* bmp_hdr = (BMP*)cluster_to_sec(clusID);
    if(bmp_hdr->bfType[0] == 0x42 && bmp_hdr->bfType[1] == 0x4d) return 1;
    return 0;
}

void display_cluster(u32 clusID)
{
    printf("Cluster %d:\n", clusID);
    u8* addr = (u8*)cluster_to_sec(clusID);
    for(int i = 0; i < CLUS_SIZE; i++)
    {
        if(i % 16 == 0) printf("\n");
        printf("%02x ", addr[i]);
    }
    printf("\n");
}

int nxt_match(int clusID, int row_size, int padding, int offset, int* nxt_clus)   //根据BMP的padding，找到接在当前clus后的最适合的cluster
{
    if(padding == 0)
    {
        *nxt_clus = clusID + 1;
        return 0;
    }
    //offset表示，当前簇中首个有效的行的偏移量，据此计算该簇尾部不完整的行的字节数
    int valid_size = CLUS_SIZE - offset;
    int valid_row = valid_size / (row_size + padding);
    int remain_size = valid_size % (row_size + padding);
    int pred_padding = row_size - remain_size;
    // pred_padding += row_size;
    // printf("row_size: %d, offset: %d, remain_size: %d, pred_padding: %d\n", row_size, offset, remain_size, pred_padding);
    *nxt_clus = clusID + 1; //default
    for(int i = 0; i < BMPdataClusNum; i++)
    {
        int cur_clus = BMPdataClus[i];
        if(cur_clus <= clusID) continue;
        u8* addr = (u8*)cluster_to_sec(cur_clus);
        int flag = 1;
        for(int k = 0; k < PAD_CHK_TIMES; k++)
        {
            for(int j = pred_padding; j < pred_padding + padding; j++)
            {
                if(addr[j] != 0x00)
                {
                    flag = 0;
                    break;
                }
            }
            if(flag == 0) break;
            pred_padding += row_size + padding;
        }
        if(flag)
        {
            // printf("Hit!\n");
            *nxt_clus = cur_clus;
            break;
        }
    }
    return remain_size;
}

void scan()
{
    // 在/tmp下创建一个临时目录，用于存放文件
    char tmpdir[512] = {0};
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/FSXXXXXX");
    mkdtemp(tmpdir);

    //计算数据区域的起始扇区
    u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
    //计算数据区域中的总扇区数
    u32 DataSecNum = hdr->BPB_TotSec32 - DataSec;
    //计算总簇数
    u32 TotalClus = DataSecNum / hdr->BPB_SecPerClus;
    int startClus = 0x2;
    int endClus = TotalClus + 1;
    printf("Total clusters: %d\n", TotalClus);

    printf("CLUS_SIZE: %d\n", CLUS_SIZE);

    //第一次扫描，找出所有的目录簇和BMP文件簇
    for(int clusID = startClus; clusID <= endClus; clusID++)
    {
        if(is_DirClus(clusID)) DirClus[DirClusNum++] = clusID;
        else if(is_BMPhdr(clusID)) BMPClus[BMPClusNum++] = clusID;
        else
        {
            u8* addr = (u8*)cluster_to_sec(clusID);
            if(addr[0] == 0x00) continue; // empty cluster
            else
            {
                BMPdataClus[BMPdataClusNum++] = clusID;
            }
        }
    }

    printf("DirClusNum: %d\n", DirClusNum);
    printf("BMPClusNum: %d\n", BMPClusNum);
    printf("BMPdataClusNum: %d\n", BMPdataClusNum);

    //第二次扫描，解析目录簇
    for(int i = 0; i < DirClusNum; i++)
    {
        int clusID = DirClus[i];
        // printf("Directory found: Cluster %d\n", clusID);
        int ndents = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(fat32dent);
        for(int d = 0; d < ndents;)
        {
            fat32dent* dent = (fat32dent*)cluster_to_sec(clusID) + d;
            char name[512] = {0};
            u32 dataClus = 0;
            int size = 0;

            assert(dent != NULL);
            if(dent->sdir.DIR_Name[0] == 0x00) break; //该簇中，所有在此目录项之后的目录项都为空
            else if(dent->sdir.DIR_Name[0] == 0xe5) {d++;  continue;} //该目录项为空
            else if(dent->ldir.LDIR_Attr == ATTR_LONG_NAME)
            {
                // printf("Long file name found\n");
                if(dent->ldir.LDIR_Ord < LAST_LONG_ENTRY) //这是一个跨簇的长文件名目录项，放弃解析
                {
                    d++;
                    continue;
                }
                //确定长文件名的长度
                int len = dent->ldir.LDIR_Ord;
                if(len > LAST_LONG_ENTRY) len ^= LAST_LONG_ENTRY; //清除0x40标志位
                if(len > ndents - 1 - d) //越界
                {
                    d++;
                    continue;
                }
                int name_len = 0;
                for(int i = 0; i < len; i++)
                {
                    for(int j = 0; j < 10; j += 2)
                    {
                        fat32dent* cur_dent = (fat32dent*)cluster_to_sec(clusID) + d + len-1-i;
                        name[name_len++] = cur_dent->ldir.LDIR_Name1[j];
                    }
                    for(int j = 0; j < 12; j += 2)
                    {
                        fat32dent* cur_dent = (fat32dent*)cluster_to_sec(clusID) + d + len-1-i;
                        name[name_len++] = cur_dent->ldir.LDIR_Name2[j];
                    }
                    for(int j = 0; j < 4; j += 2)
                    {
                        fat32dent* cur_dent = (fat32dent*)cluster_to_sec(clusID) + d + len-1-i;
                        name[name_len++] = cur_dent->ldir.LDIR_Name3[j];
                    }
                }
                name[name_len] = '\0';
                fat32dent* sdent = (fat32dent*)cluster_to_sec(clusID) + d + len;
                dataClus = sdent->sdir.DIR_FstClusLO | (sdent->sdir.DIR_FstClusHI << 16);
                size = sdent->sdir.DIR_FileSize;
                d += len + 1;
            }
            else 
            {
                // printf("Normal file name found\n");
                if(dent->sdir.DIR_NTRes != 0 || dent->sdir.DIR_Name[0] == 0x00 || dent->sdir.DIR_Attr != ATTR_DIRECTORY)
                {
                    d++;
                    continue;
                }
                get_filename(dent, name);
                dataClus = dent->sdir.DIR_FstClusLO | (dent->sdir.DIR_FstClusHI << 16);
                size = dent->sdir.DIR_FileSize;
                d++;
            }
            //开始恢复文件
            u8* addr = (u8*)cluster_to_sec(dataClus);
            if(addr == NULL) continue;
            BMP* bmp_hdr = (BMP*)addr;
            if(bmp_hdr->bfType[0] == 0x42 && bmp_hdr->bfType[1] == 0x4d)
            {
                //计算padding
                int row_size = bmp_hdr->biWidth * bmp_hdr->biBitCount / 8;
                // printf("biWidth: %d, biBitCount: %d\n", bmp_hdr->biWidth, bmp_hdr->biBitCount);
                int padding = (4 - row_size % 4) % 4;
                // printf("Padding: %d\n", padding);

                //在之前创建的临时目录下创建一个文件，用于存放BMP文件
                char bmp_filename[2048] = {0};
                snprintf(bmp_filename, sizeof(bmp_filename), "%s/%s.bmp", tmpdir, name);
                int bmp_fd = open(bmp_filename, O_RDWR | O_CREAT, 0666);
                if(bmp_fd < 0)
                {
                    perror("open");
                    continue;
                }
                // display_cluster(dataClus);
                int total_clust = ceil(size, CLUS_SIZE);
                // printf("Total clusters: %d\n", total_clust);
                int cur_clust = dataClus;   
                int nxt_clust = cur_clust + 1;
                // display_cluster(nxt_clust);
                int remain_size = size;
                int offset = bmp_hdr->bfOffBits;
                while(total_clust-- && remain_size > 0)
                {
                    // printf("remain_size: %d\n", remain_size);
                    int size_to_write = min(remain_size, CLUS_SIZE);
                    // printf("size_to_write: %d\n", size_to_write);
                    write(bmp_fd, addr, size_to_write);
                    remain_size -= size_to_write;
                    if(remain_size == 0) break;
                    int rest = nxt_match(cur_clust, row_size, padding, offset, &nxt_clust);
                    offset = row_size + padding - rest;
                    // printf("cur_clust: %d, nxt_clust: %d\n", cur_clust, nxt_clust);
                    cur_clust = nxt_clust;
                    // addr = addr + size_to_write;
                    addr = (u8*)cluster_to_sec(nxt_clust);
                    // printf("addr: %p, addr2: %p\n", addr, addr2);
                }
                // write(bmp_fd, addr, size);
                close(bmp_fd);
                //通过popen执行sha1sum命令，计算BMP文件的SHA1值
                char sha1sum_cmd[4096] = {0};
                snprintf(sha1sum_cmd, sizeof(sha1sum_cmd), "sha1sum %s", bmp_filename);
                FILE* sha1sum_fp = popen(sha1sum_cmd, "r");
                if(sha1sum_fp == NULL)
                {
                    perror("popen");
                    continue;
                }
                char sha1sum[41] = {0};
                fread(sha1sum, 1, 40, sha1sum_fp);
                pclose(sha1sum_fp);
                //输出SHA1值
                printf("%s         %s\n", sha1sum, name);
                fflush(stdout);
            }
        }
    }
}