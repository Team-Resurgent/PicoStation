/*

MIT License

Copyright (c) 2020 PCSX-Redux authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "posix_file.h"
#include "ff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void posix_destroy(struct CueFile *file) {}

static void posix_close(struct CueFile *file, struct CueScheduler *scheduler, void (*cb)(struct CueFile *, struct CueScheduler *)) {
    FIL *fp = (FIL *)file->opaque;
    
    if (!fp)
	{
		return;
	}
	
    if (fp->cltbl)
    {
		free(fp->cltbl);
	}
    f_close(fp);
    free(fp);
    
    if (scheduler)
	{
		File_schedule_close(file, scheduler, cb);
	}
}

static void posix_size(struct CueFile *file, struct CueScheduler *scheduler, int compressed,
                       void (*cb)(struct CueFile *, struct CueScheduler *, uint64_t)) {
    FIL *f = (FIL *)file->opaque;
    uint64_t size = f_size(f);
    File_schedule_size(file, scheduler, size, cb);
}

static void posix_read(struct CueFile *file, struct CueScheduler *scheduler, uint32_t amount, uint64_t cursor,
                       uint8_t *buffer,
                       void (*cb)(struct CueFile *, struct CueScheduler *, int error, uint32_t amount, uint8_t *buffer)) {
    
    FIL *f = (FIL *)file->opaque;
    size_t r;
    f_lseek(f, cursor);
    int ret = f_read(f, buffer, amount, &r);
    
    File_schedule_read(file, scheduler, ret == 0 ? 0 : 1, r, buffer, cb);
}

static void posix_write(struct CueFile *file, struct CueScheduler *scheduler, uint32_t amount, uint64_t cursor,
                        const uint8_t *buffer,
                        void (*cb)(struct CueFile *, struct CueScheduler *, int error, uint32_t amount)) {
#if FF_FS_READONLY == 0
    FIL *f = (FIL *)file->opaque;
    size_t r;
    f_lseek(f, cursor);
    int ret = f_write(f, buffer, amount, &r);
    
    File_schedule_write(file, scheduler, ret == 0 ? 0 : 1, r, cb);
#endif
}

struct CueFile *create_posix_file(struct CueFile *file, const char *filename, uint8_t mode) {
    
    FIL *fp = malloc(sizeof(FIL));
    if (!fp)
	{
		return NULL;
	}
	
    if(f_open(fp, filename, mode))
    {
		free(fp);
		return NULL;
	}
    
    DWORD cltbltmp = 1;
    fp->cltbl = &cltbltmp;
    
    FRESULT r = f_lseek(fp, CREATE_LINKMAP);
    
    if (r == FR_NOT_ENOUGH_CORE)
	{
		size_t count = fp->cltbl[0];
		DWORD *cltbl = (DWORD *)malloc(count * sizeof(DWORD));
		
		if (cltbl)
		{
			memset(cltbl, 0, count * sizeof(DWORD));
			cltbl[0] = count;
			fp->cltbl = cltbl;
			r = f_lseek(fp, CREATE_LINKMAP);
			
			if(r != FR_OK)
			{
				free(cltbl);
			}
		}
	}
    
    if(r != FR_OK)
	{
		fp->cltbl = NULL;
	}
    
    file->opaque = fp;
    file->destroy = posix_destroy;
    file->close = posix_close;
    file->size = posix_size;
    file->read = posix_read;
    file->write = posix_write;
    file->cfilename = NULL;
    file->filename = NULL;
    file->references = 1;
    return file->opaque ? file : NULL;
}
