#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

struct fat32hdr {
    u8  BS_jmpBoot[3];
    u8  BS_OEMName[8];
    u16 BPB_BytsPerSec;
    u8  BPB_SecPerClus;
    u16 BPB_RsvdSecCnt;
    u8  BPB_NumFATs;
    u16 BPB_RootEntCnt;
    u16 BPB_TotSec16;
    u8  BPB_Media;
    u16 BPB_FATSz16;
    u16 BPB_SecPerTrk;
    u16 BPB_NumHeads;
    u32 BPB_HiddSec;
    u32 BPB_TotSec32;
    u32 BPB_FATSz32;
    u16 BPB_ExtFlags;
    u16 BPB_FSVer;
    u32 BPB_RootClus;
    u16 BPB_FSInfo;
    u16 BPB_BkBootSec;
    u8  BPB_Reserved[12];
    u8  BS_DrvNum;
    u8  BS_Reserved1;
    u8  BS_BootSig;
    u32 BS_VolID;
    u8  BS_VolLab[11];
    u8  BS_FilSysType[8];
    u8  __padding_1[420];
    u16 Signature_word;
} __attribute__((packed));

typedef struct SDIR {
    u8  DIR_Name[11];
    u8  DIR_Attr;
    u8  DIR_NTRes;
    u8  DIR_CrtTimeTenth;
    u16 DIR_CrtTime;
    u16 DIR_CrtDate;
    u16 DIR_LastAccDate;
    u16 DIR_FstClusHI;
    u16 DIR_WrtTime;
    u16 DIR_WrtDate;
    u16 DIR_FstClusLO;
    u32 DIR_FileSize;
} __attribute__((packed)) SDIR;

typedef struct LDIR
{
    u8 LDIR_Ord;
    u8 LDIR_Name1[10];
    u8 LDIR_Attr;
    u8 LDIR_Type;
    u8 LDIR_Chksum;
    u8 LDIR_Name2[12];
    u16 LDIR_FstClusLO;
    u8 LDIR_Name3[4];
} __attribute__((packed)) LDIR;

typedef union fat32dent
{
    SDIR sdir;
    LDIR ldir;
} fat32dent;

typedef struct BMP
{
    u8 bfType[2];
    u32 bfSize;
    u16 bfReserved1;
    u16 bfReserved2;
    u32 bfOffBits;
    u32 biSize;
    u32 biWidth;
    u32 biHeight;
    u16 biPlanes;
    u16 biBitCount;
    u32 biCompression;
    u32 biSizeImage;
    u32 biXPelsPerMeter;
    u32 biYPelsPerMeter;
    u32 biClrUsed;
    u32 biClrImportant;
} __attribute__((packed)) BMP;

#define CLUS_INVALID   0xffffff7

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define LAST_LONG_ENTRY 0x40
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)