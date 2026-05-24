#ifndef __EXFUNS_H
#define __EXFUNS_H

#include <stdint.h>
#include "ff.h"

extern FATFS *fs[_VOLUMES];
extern FIL *file;
extern FIL *ftemp;
extern UINT br, bw;
extern FILINFO fileinfo;
extern DIR dir;
extern uint8_t *fatbuf;

#define T_BIN   0X00
#define T_LRC   0X10

#define T_NES   0X20
#define T_SMS   0X21

#define T_TEXT  0X30
#define T_C     0X31
#define T_H     0X32

#define T_WAV   0X40
#define T_MP3   0X41
#define T_APE   0X42
#define T_FLAC  0X43

#define T_BMP   0X50
#define T_JPG   0X51
#define T_JPEG  0X52
#define T_GIF   0X53

#define T_AVI   0X60

uint8_t exfuns_init(void);
uint8_t f_typetell(uint8_t *fname);
uint8_t exf_getfree(uint8_t *drv, uint32_t *total, uint32_t *free);
uint32_t exf_fdsize(uint8_t *fdname);
uint8_t *exf_get_src_dname(uint8_t *dpfn);
uint8_t exf_copy(uint8_t (*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode),
                 uint8_t *psrc, uint8_t *pdst, uint32_t totsize, uint32_t cpdsize, uint8_t fwmode);
uint8_t exf_fdcopy(uint8_t (*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode),
                   uint8_t *psrc, uint8_t *pdst, uint32_t *totsize, uint32_t *cpdsize, uint8_t fwmode);

#endif
