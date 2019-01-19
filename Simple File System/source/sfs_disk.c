#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <err.h>

#include "sfs_types.h"
#include "sfs_disk.h"

#define BLOCKSIZE  512

#ifndef EINTR
#define EINTR 0
#endif

static int fd=-1;

void
disk_open(const char *path)
{
	assert(fd<0);
	fd = open(path, O_RDWR);

	if (fd<0) {
		err(1, "%s", path);
	}
}

u_int32_t
disk_blocksize(void)
{
	assert(fd>=0);
	return BLOCKSIZE;
}

void
disk_write(const void *data, u_int32_t block)
{
	const char *cdata = data;
	u_int32_t tot=0;
	int len;

	assert(fd>=0);

	if (lseek(fd, block*BLOCKSIZE, SEEK_SET)<0) {
		err(1, "lseek");
	}

	while (tot < BLOCKSIZE) {
		len = write(fd, cdata + tot, BLOCKSIZE - tot);
		if (len < 0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			err(1, "write");
		}
		if (len==0) {
			err(1, "write returned 0?");
		}
		tot += len;
	}
}


/*
 * 읽고 싶은 i-node 구조체와 그 블록의 위치를 주면 구조체에 데이터를 써준다. 
 * 그래서 주소를 넘겨주고 쓰고 데이터를 넣어주는구나!!!!!!!!!!!!!!!!
 */
void
disk_read(void *data, u_int32_t block)
{

	// *data = spb
	// block = 0 (# of spb location)
	char *cdata = data;
	u_int32_t tot=0;
	int len;

	assert(fd>=0);
	
	/*
	 * SEEK_SET : 파일의 처음을 기준한다. 
	 * fd부터 block*BLOCKSIZE 만큼 위치를 변경한다.
	 * 즉 block번째 블록이 파일을 읽는 첫 위치이다.
	 * 성공시 파일의 시작부터 떨어진 byte만큼의 block 리턴.
	 * 실패시 -1 리턴
	 */
	if (lseek(fd, block*BLOCKSIZE, SEEK_SET)<0) {
		err(1, "lseek");
	}

	while (tot < BLOCKSIZE) {
		len = read(fd, cdata + tot, BLOCKSIZE - tot);
		if (len < 0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			err(1, "read");
		}
		if (len==0) {
			err(1, "unexpected EOF in mid-sector");
		}
		tot += len;
	}
}

void
disk_close(void)
{
	assert(fd>=0);
	if (close(fd)) {
		err(1, "close");
	}
	fd = -1;
}
