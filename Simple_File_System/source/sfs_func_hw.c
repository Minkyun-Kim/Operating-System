//
// Simple FIle System
// Student Name : Minkyun Kim
// Student Number : B411027
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1 << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1 << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1 << (b)))
#define BIT_CHECK(a, b) ((a) & (1 << (b)))


u_int32_t cpintotouch = 0;

void print_list(u_int32_t, char[]);
int rtn_allocate_free_bitmap(int);
void make_free_blockNumber(u_int32_t , int);
int cp_read(FILE*, void*, u_int32_t);
u_int32_t num_of_free_blocks();
u_int32_t getfilesize(const char*);
void print_indirect(struct sfs_inode);
u_int32_t touchFileFail;
u_int32_t touchNewFileInode;

static struct sfs_super spb;				// superblock
static struct sfs_dir sd_cwd = {SFS_NOINO}; // current working directory

//////////////////////////////
// [COMPLETE] error_message //
//////////////////////////////
void error_message(const char *message, const char *path, int error_code)
{
	switch (error_code)
	{
	case -1:
		printf("%s: %s: No such file or directory\n", message, path);
		return;
	case -2:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -3:
		printf("%s: %s: Directory full\n", message, path);
		return;
	case -4:
		printf("%s: %s: No block available\n", message, path);
		return;
	case -5:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -6:
		printf("%s: %s: Already exists\n", message, path);
		return;
	case -7:
		printf("%s: %s: Directory not empty\n", message, path);
		return;
	case -8:
		printf("%s: %s: Invalid argument\n", message, path);
		return;
	case -9:
		printf("%s: %s: Is a directory\n", message, path);
		return;
	case -10:
		printf("%s: %s: Is not a file\n", message, path);
		return;
	case -11:
		printf("%s: can't open %s input file\n", message, path);
		return;
	case -12:
		printf("%s: input file size exceeds the max file size\n", message);
		return;
	case -13:
		printf("%s: can't make %s file\n", message, path);
		return;
	default:
		printf("unknown error code\n");
		return;
	}
}

//////////////////////////
// [COMPLETE] sfs_mount //
//////////////////////////
void sfs_mount(const char *path)
{
	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read(&spb, SFS_SB_LOCATION);

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert(spb.sp_magic == SFS_MAGIC);

	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);

	sd_cwd.sfd_ino = 1; //init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}
///////////////////////////
// [COMPLETE] sfs_umount //
///////////////////////////
void sfs_umount()
{

	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

///////////////////////////
// [COMPLETE] sfs_touch //
///////////////////////////
void sfs_touch(const char *path)
{
	//skeleton implementation
	//read current I-node
	int i, j;
	struct sfs_inode curInode;
	bzero(&curInode, sizeof(struct sfs_inode));
	disk_read(&curInode, sd_cwd.sfd_ino);

	//path와 같은 이름이 있는 경우
	for(i = 0; i < SFS_NDIRECT;i++){
		if(curInode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_NDIRECT; j++){
			if((dirBlock[j].sfd_ino != 0) &&!strcmp(dirBlock[j].sfd_name, path)){
				touchFileFail = 1;
				error_message("touch", path, -6);
				return;
			}
		}
	}

	//if(curInode.sfi_size == SFS_NDIRECT * SFS_BLOCKSIZE){
	//		error_message("touch", path, -4);
	//}

	//path와 같은 이름이 없는 경우
	for(i = 0; i < SFS_NDIRECT; i++){
		//Direct pointer is valid
		j = 0;
		if(curInode.sfi_direct[i] != 0){

			struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
			bzero(&dirBlock, sizeof(dirBlock));
			disk_read(&dirBlock, curInode.sfi_direct[i]);
			for(; j < SFS_DENTRYPERBLOCK; j++){
				//현재 디렉토리에 파일 추가 가능
				if(dirBlock[j].sfd_ino == 0){
					int n;
					// 할당 가능한 free block이 없음
					if((n = rtn_allocate_free_bitmap(1)) == -1){
						touchFileFail = 1;
						if(!cpintotouch){
							error_message("touch", path, -4);
						}
						cpintotouch = 0;

						return;
					}
					
					//할당 가능한 free block이 있음
					else{

						/*
						 * 1. directory block에 i-node 번호 써주기
						 * 2. directory block에 name 써주기
						 * 3. disk_write
						 * 4. i-node 구조체 만들기
						 * 5. 초기화
						 * 6. TYPE == FILE
						 * 7. disk_write
						 * 8. cwd size += sizeof(struct sfs_dir);
						 * 9. return
						 */
						dirBlock[j].sfd_ino = n; // 1
						strncpy(dirBlock[j].sfd_name, path, SFS_NAMELEN); // 2
						struct sfs_inode newFileInode; // 4
						bzero(&newFileInode, sizeof(struct sfs_inode)); //5
						newFileInode.sfi_type = SFS_TYPE_FILE; // 6
						curInode.sfi_size += sizeof(struct sfs_dir); // 8
						disk_write(&dirBlock, curInode.sfi_direct[i]); // 3
						disk_write(&newFileInode, n); // 7
						disk_write(&curInode, sd_cwd.sfd_ino);// 9
						touchNewFileInode = n;
						
						return;
					}
				}
				//끝까지 탐색했는데 추가할 공간이 없음
				if((i == SFS_NDIRECT-1) && (j == SFS_DENTRYPERBLOCK-1)){
					touchFileFail = 1;
					error_message("touch", path, -3);
					return;
				}
			}
		}

		//DIRECT pointer is empty
		//i-node새로 할당해줘야함
		else{
			int n, m;
			//더이상 할당할 free block이 없다.
			if( num_of_free_blocks() < 2){
				if(!cpintotouch){
					error_message("touch", path, -4);
				}
				cpintotouch = 0;

				return;
			}
			//할당가능하다
			/*
				* 1. Direct ptr에 dirBlock번호 쓰기
				* 2. size += 64
				* 3. cwd 블록 disk_write
				* 4. dir block 구조체 선언
				* 5. 초기화
				* 6. i-node 번호 하나더 받아오기
				* 7. i-node쓰기
				* 8. name 쓰기
				* 9. disk_write
				* 10. 새로 받은 i-node구조체 만들기
				* 11. 초기화
				* 12. TYPE = FILE
				* 13. disk_write
				* 14. return;
				*/

			n = rtn_allocate_free_bitmap(1);
			curInode.sfi_direct[i] = n; // 1
			curInode.sfi_size += sizeof(struct sfs_dir); // 2
			struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK]; // 4
			bzero(&dirBlock, sizeof(dirBlock)); //5
			m = rtn_allocate_free_bitmap(1); // 6
			dirBlock[0].sfd_ino = m; // 7
			strncpy(dirBlock[0].sfd_name, path, SFS_NAMELEN); // 8
			struct sfs_inode newFileInode; // 10
			bzero(&newFileInode, sizeof(struct sfs_inode)); // 11
			newFileInode.sfi_type = SFS_TYPE_FILE; // 12
			disk_write(&curInode, sd_cwd.sfd_ino); //3
			disk_write(&dirBlock, n); // 9
			disk_write(&newFileInode, m); // 13
			touchNewFileInode = m;

			return;				
			
		}
	}
}

///////////////////////
// [COMPLETE] sfs_cd //
///////////////////////
void sfs_cd(const char *path)
{
	int i, j;
	//cd 만 입력된 경우 root로 돌아간다.
	if (path == NULL)
	{
		sd_cwd.sfd_ino = 1;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}
	//경로가 넘어온 경우
	else
	{
		int exist = 0; // 0 : PATH doesn't exist, 1 : PATH exist
		//u_int32_t curInodeNum = sd_cwd.sfd_ino;
		struct sfs_inode curInode;
		bzero(&curInode, sizeof(struct sfs_inode));
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));
		disk_read(&curInode, sd_cwd.sfd_ino);
		//search if there is a PATH FILE
		for (i = 0; i < SFS_NDIRECT; ++i)
		{
			if (curInode.sfi_direct[i] == 0)
				continue;
			disk_read(&dirBlock, curInode.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; ++j)
			{
				if (dirBlock[j].sfd_ino == 0)
					continue;
				//if PATH found
				if ((dirBlock[j].sfd_ino != 0) && !strcmp(dirBlock[j].sfd_name, path))
				{
					exist = 1;
					struct sfs_inode pathInode;
					//read to get subfile TYPE
					disk_read(&pathInode, dirBlock[j].sfd_ino);
					//path is FILE
					if (pathInode.sfi_type == SFS_TYPE_FILE)
					{
						error_message("cd", path, -2);
						return;
					}
					//path is DIRECTORY
					else
					{
						sd_cwd.sfd_ino = dirBlock[j].sfd_ino;
						strncpy(sd_cwd.sfd_name, dirBlock[j].sfd_name, SFS_NAMELEN);
						return;
					}
				}
			}
		}
		if (!exist)
			error_message("cd", path, -1);
	}
}

///////////////////////
// [COMPLETE] sfs_ls //
///////////////////////
void sfs_ls(const char *path)
{
	/* 
	 *  1. path가 없으면 현재 디렉토리를 출력한다.
	 *  2. path의 dir뒤는 '/'를 출력한다.
	 *  err : 파일이 없는 경우
	 */
	int i, j, p, q;

	//인자가 없는 경우
	if (path == NULL)
	{

		print_list(sd_cwd.sfd_ino, NULL);//path <---- NULL

	}
	//인자가 있는 경우
	else
	{
		int exist = 0; // 0 : PATH doesn't exist, 1 : PATH exist
		//u_int32_t curInodeNum = sd_cwd.sfd_ino;
		struct sfs_inode curInode;
		bzero(&curInode, sizeof(struct sfs_inode));
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));

		disk_read(&curInode, sd_cwd.sfd_ino);

		//search if there is a PATH FILE
		for (i = 0; i < SFS_NDIRECT; ++i)
		{
			if (curInode.sfi_direct[i] == 0)
				continue;

			disk_read(&dirBlock, curInode.sfi_direct[i]);

			for (j = 0; j < SFS_DENTRYPERBLOCK; ++j)
			{
				if (dirBlock[j].sfd_ino == 0)
					continue;
				//if PATH found
				if ((dirBlock[j].sfd_ino != 0) && !strcmp(dirBlock[j].sfd_name, path))
				{
					exist = 1;
					print_list(dirBlock[j].sfd_ino, dirBlock[j].sfd_name);
					return;
				}
			}
		}
		if (!exist)
			error_message("ls", path, -1);
	}
}

//////////////////////////
// [COMPLETE] sfs_mkdir //
//////////////////////////
void sfs_mkdir(const char *org_path)
{
	int i, j;
	struct sfs_inode curInode;
	disk_read(&curInode, sd_cwd.sfd_ino);
	//같은 이름을 가진 파일이나 폴더가 있는지 확인
	for(i = 0; i < SFS_NDIRECT; i++){
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if((dirBlock[j].sfd_ino != 0 ) && !strcmp(dirBlock[j].sfd_name, org_path)){
				error_message("mkdir", org_path, -6);
				return;
			}
		}
	}

	//같은 이름은 없음. 현재 디렉토리에 남는 자리가 있는지, 할당가능한 블록이 있는지
	for(i = 0; i < SFS_NDIRECT; i++){
		j = 0;
		//direct ptr가 있어 거기서 할당 가능.
		if(curInode.sfi_direct[i] != 0){
			struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
			bzero(&dirBlock, sizeof(dirBlock));
			disk_read(&dirBlock, curInode.sfi_direct[i]);
			for(; j < SFS_DENTRYPERBLOCK; j++){
				//현재 디렉토리에 파일 추가 가능
				if(dirBlock[j].sfd_ino == 0){
					int n, m;
					// 할당 가능한 free block이 없음
					if((n = rtn_allocate_free_bitmap(1)) == -1){
						error_message("mkdir", org_path, -4);
						return;
					}
					//할당 가능한 free block이 있음
					else{
						/*
						 * 1. directory block에 i-node 번호 써주기
						 * 2. directory block에 name 써주기
						 * 3. disk_write
						 * 4. i-node 구조체 만들기
						 * 5. 초기화
						 * 6. TYPE == FILE
						 * 7. disk_write
						 * 8. cwd size += sizeof(struct sfs_dir);
						 * 9. return
						 * 10. 새로운 Dir Block 구조체 만들기
						 * 11. 초기화
						 * 12. 새로운 Dir Block 번호 가져오기
						 * 13. 새로운 dir에 해당 Dir Block number 넣어주기
						 * 14. 해당 i-node 구조체에 . 이랑 name 추가하기
						 * 15. 해당 i-node 구조체에 .. 이랑 name 추가하기
						 * 16. disk_write
						 */
						dirBlock[j].sfd_ino = n; // 1
						strncpy(dirBlock[j].sfd_name, org_path, SFS_NAMELEN); // 2
						struct sfs_inode newDirInode; // 4
						bzero(&newDirInode, sizeof(struct sfs_inode)); //5
						newDirInode.sfi_type = SFS_TYPE_DIR; // 6
						curInode.sfi_size += sizeof(struct sfs_dir); // 8
						struct sfs_dir newDirBlock[SFS_DENTRYPERBLOCK]; // 10
						bzero(&newDirBlock, sizeof(newDirBlock)); // 11
						if((m = rtn_allocate_free_bitmap(1)) == -1){ // 12
							make_free_blockNumber(n, 1);
							error_message("mkdir", org_path, -4);
						}
						newDirInode.sfi_direct[0] = m; // 13
						newDirBlock[0].sfd_ino = n; // 14
						strncpy(newDirBlock[0].sfd_name, ".", SFS_NAMELEN); // 14
						newDirBlock[1].sfd_ino = sd_cwd.sfd_ino; // 15
						strncpy(newDirBlock[1].sfd_name, "..", SFS_NAMELEN); // 15
						newDirInode.sfi_size += 2 * sizeof(struct sfs_dir);

						int q;
						for(q = 2; q < SFS_DENTRYPERBLOCK; q++){
							newDirBlock[q].sfd_ino = SFS_NOINO;
						}
						disk_write(&newDirBlock, m); // 16
						disk_write(&dirBlock, curInode.sfi_direct[i]); // 3
						disk_write(&newDirInode, n); // 7
						disk_write(&curInode, sd_cwd.sfd_ino);// 9
						return;
					}
				}
				//끝까지 탐색했는데 추가할 공간이 없음
				if((i == SFS_NDIRECT-1) && (j == SFS_DENTRYPERBLOCK-1)){
					error_message("mkdir", org_path, -3);
					return;
				}
			}
		}
		else{
			int n, m, p;
			//더이상 할당할 free block이 없다.
			if((n = rtn_allocate_free_bitmap(1)) == -1){
				error_message("mkdir", org_path, -4);
				return;
			}
			//할당가능하다
			else{
				/*
				 * 1. Direct ptr에 dirBlock번호 쓰기
				 * 2. size += 64
				 * 3. cwd 블록 disk_write
				 * 4. dir block 구조체 선언
				 * 5. 초기화
				 * 6. i-node 번호 하나더 받아오기
				 * 7. i-node쓰기
				 * 8. name 쓰기
				 * 9. disk_write
				 * 10. 새로 받은 i-node구조체 만들기
				 * 11. 초기화
				 * 12. TYPE = FILE
				 * 13. disk_write
				 * 14. 새로운 Dir Block 구조체 만들기
				 * 15. 초기화
				 * 16. 새로운 Dir Block 번호 가져오기
				 * 17. 새로운 dir에 해당 Dir Block number 넣어주기
				 * 18. 해당 i-node 구조체에 . 이랑 name 추가하기
				 * 19. 해당 i-node 구조체에 .. 이랑 name 추가하기
				 * 20. disk_write
				 * 23. return;
				 */
				curInode.sfi_direct[i] = n; // 1
				curInode.sfi_size += sizeof(struct sfs_dir); // 2
				struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK]; // 4
				bzero(&dirBlock, sizeof(dirBlock)); //5
				if((m = rtn_allocate_free_bitmap(1)) == -1){ // 6
					make_free_blockNumber(n, 1);//뒤늦게 블록이 부족하면 이전것도 해제해줘야함
					error_message("touch", org_path, -4);
					return;
				}
				dirBlock[0].sfd_ino = m; // 7
				strncpy(dirBlock[0].sfd_name, org_path, SFS_NAMELEN); // 8
				struct sfs_inode newDirInode; // 10
				bzero(&newDirInode, sizeof(struct sfs_inode)); // 11
				newDirInode.sfi_type = SFS_TYPE_DIR; // 12
				struct sfs_dir newDirBlock[SFS_DENTRYPERBLOCK]; // 14
				bzero(&newDirBlock, sizeof(newDirBlock)); // 15
				if((p = rtn_allocate_free_bitmap(1)) == -1){ // 16
					make_free_blockNumber(n, 1); //뒤늦게 블록이 부족하면 이전것도 해제해줘야함
					make_free_blockNumber(m, 1);//뒤늦게 블록이 부족하면 이전것도 해제해줘야함
					error_message("mkdir", org_path, -4);
					return;
				}
				newDirInode.sfi_direct[0] = p; // 17
				newDirBlock[0].sfd_ino = m; // 18
				strncpy(newDirBlock[0].sfd_name, ".", SFS_NAMELEN); // 19
				newDirBlock[1].sfd_ino = sd_cwd.sfd_ino; // 20
				strncpy(newDirBlock[1].sfd_name, "..", SFS_NAMELEN); // 21
				newDirInode.sfi_size += 2 * sizeof(struct sfs_dir);
				
				int q;
				for(q = 2; q < SFS_DENTRYPERBLOCK; q++){
					newDirBlock[q].sfd_ino = SFS_NOINO;
				}
				
				disk_write(&newDirBlock, p); // 22
				disk_write(&curInode, sd_cwd.sfd_ino); //3
				disk_write(&dirBlock, n); // 9
				disk_write(&newDirInode, m); // 13
				return;				
			}
		}
	}
}

//////////////////////////
// [COMPLETE] sfs_rmdir //
//////////////////////////
void sfs_rmdir(const char *org_path)
{
	// "."에 대해서는 에러처리
	if(!strcmp(org_path, ".")){
		error_message("rmdir", org_path, -8);
		return;
	}

	int i, j;
	struct sfs_inode curInode;
	disk_read(&curInode, sd_cwd.sfd_ino);
	for (i = 0; i < SFS_NDIRECT; i++){
		if(curInode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			//삭제할 디렉토리명을 찾음. 디렉토리인지 파일인지는 찾아봐야함
			if((dirBlock[j].sfd_ino != 0) && !strcmp(dirBlock[j].sfd_name, org_path)){
				struct sfs_inode dirOrFile;
				disk_read(&dirOrFile, dirBlock[j].sfd_ino);
				//삭제하려는게 디렉토리이면 디렉토리가 비어있는지 확인후 삭제한다.
				if(dirOrFile.sfi_type == SFS_TYPE_DIR){
					int p, q;
					for(p = 0; p < SFS_NDIRECT; p++){
						if(dirOrFile.sfi_direct[p] == 0)
							continue;
						struct sfs_dir ifEmptyBlock[SFS_DENTRYPERBLOCK];
						disk_read(&ifEmptyBlock, dirOrFile.sfi_direct[p]);
						if(p == 0){
							for(q = 2; q < SFS_DENTRYPERBLOCK; q++){
								if(ifEmptyBlock[q].sfd_ino != 0){
									error_message("rmdir", org_path, -7);
									return;
								}
							}
						}
						else{
							for(q = 0; q < SFS_DENTRYPERBLOCK; q++){
								if(ifEmptyBlock[q].sfd_ino != 0){
									error_message("rmdir", org_path, -7);
									return;
								}
							}
						}
					}
					make_free_blockNumber(dirBlock[j].sfd_ino, 0);

					dirBlock[j].sfd_ino = SFS_NOINO;
					curInode.sfi_size -= sizeof(struct sfs_dir);
					disk_write(&dirBlock, curInode.sfi_direct[i]);
					return;
				}
				//삭제하려는게 디렉토리가 아니면
				else{
					error_message("rmdir", org_path, -5);
					return;
				}
			}
		}
	}
	error_message("rmdir", org_path, -1);
	return;
}

///////////////////////
// [COMPLETE] sfs_mv //
///////////////////////
void sfs_mv(const char *src_name, const char *dst_name)
{
	int i, j;
	struct sfs_inode inode;
	bzero(&inode, sizeof(struct sfs_inode));
	disk_read(&inode, sd_cwd.sfd_ino);
	//search weather dst_name already exists
	for (i = 0; i < SFS_NDIRECT; i++){
		if (inode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));
		disk_read(&dirBlock, inode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if ((dirBlock[j].sfd_ino != 0) && !strcmp(dirBlock[j].sfd_name, dst_name)){
				error_message("mv", dst_name, -6);
				return;
			}
		}
	}

	//find src_name to change name.
	for (i = 0; i < SFS_NDIRECT; i++){
		if (inode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));
		disk_read(&dirBlock, inode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if ((dirBlock[j].sfd_ino != 0 ) && !strcmp(dirBlock[j].sfd_name, src_name)){
				strncpy(dirBlock[j].sfd_name, dst_name, SFS_NAMELEN);
				disk_write(&dirBlock, inode.sfi_direct[i]);
				return;
			}
		}
	}
	//no src_name
	error_message("mv", src_name, -1);
	return;
}

///////////////////////
// [COMPLETE] sfs_rm //
///////////////////////
void sfs_rm(const char *path)
{
	int i, j;
	struct sfs_inode curInode;
	disk_read(&curInode, sd_cwd.sfd_ino);
	for (i = 0; i < SFS_NDIRECT; i++){
		if(curInode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			//삭제할 파일명을 찾음. 디렉토리인지 파일인지는 찾아봐야함
			if((dirBlock[j].sfd_ino != 0) && !strcmp(dirBlock[j].sfd_name, path)){
				int p;
				struct sfs_inode fileInode;
				disk_read(&fileInode, dirBlock[j].sfd_ino);
				//error if path is DIR
				if(fileInode.sfi_type == SFS_TYPE_DIR){
					error_message("rm", path, -9);
					return;
				}

				//free direct block
				for(p = 0; p < SFS_NDIRECT; p++){
					if(fileInode.sfi_direct[p] == 0){
						continue;
					}

					make_free_blockNumber(fileInode.sfi_direct[p], 1);
					fileInode.sfi_direct[p] = SFS_NOINO;
				}
				disk_write(&fileInode, dirBlock[j].sfd_ino);

				//free indirect block 
				if(fileInode.sfi_indirect != 0){
					u_int32_t indirectBlock[SFS_DBPERIDB];
					disk_read(&indirectBlock, fileInode.sfi_indirect);
					for(p = 0; p < SFS_DBPERIDB; p++){
						if(indirectBlock[p] == 0){
							continue;
						}

						make_free_blockNumber(indirectBlock[p], 1);
						indirectBlock[p] = SFS_NOINO;
					}

					make_free_blockNumber(fileInode.sfi_indirect, 1);//////////////추가
					disk_write(&indirectBlock, fileInode.sfi_indirect);
					fileInode.sfi_indirect = SFS_NOINO;////////이렇게 추가
					disk_write(&fileInode, dirBlock[j].sfd_ino);//////////////이렇게 추가
					
				}
				//free file

				make_free_blockNumber(dirBlock[j].sfd_ino,1);
				dirBlock[j].sfd_ino = SFS_NOINO;
				disk_write(&dirBlock, curInode.sfi_direct[i]);
				return;
			}
		}
	}
	error_message("rm", path, -1);
	return;
}

/////////////////////////
// [COMPLETE] sfs_cpin //
/////////////////////////
void sfs_cpin(const char *local_path, const char *path)
{
	FILE* fd;
	//소스 파일이 없으면 에러 리턴
	if( (fd= fopen(path, "rb")) == NULL){
		error_message("cpin", path, -11);
		return;
	}
	int i, j;
	struct sfs_inode curInode;
	struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
	disk_read(&curInode, sd_cwd.sfd_ino);
	// local_path가 이미 있으면 에러리턴
	for(i = 0; i < SFS_NDIRECT; i++){
		if(curInode.sfi_direct[i] == 0){
			continue;
		}
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(dirBlock[j].sfd_ino != 0 && !strcmp(dirBlock[j].sfd_name, local_path)){
				error_message("cpin", local_path, -6);
				return;
			}
		}
	}
	
	char buf[SFS_NDIRECT + SFS_DBPERIDB][SFS_BLOCKSIZE];
	bzero(buf, sizeof(buf));
	//파일의 크기 구하기
	u_int32_t filesize;
	filesize = getfilesize(path);
	u_int32_t blockNeeded = SFS_ROUNDUP(filesize, SFS_BLOCKSIZE) / SFS_BLOCKSIZE;
	//파일 용량이 커서 SFS시스템에 담을 수 없는경우
	if(filesize > (SFS_NDIRECT + SFS_DBPERIDB)*SFS_BLOCKSIZE){
		error_message("cpin", NULL, -12);
		fclose(fd);
		return;
	}
	
	//필요한 Inode갯수 구하기
	u_int32_t neededInode = 0;
	neededInode = SFS_ROUNDUP(filesize, SFS_BLOCKSIZE) / SFS_BLOCKSIZE;
	if(neededInode > SFS_NDIRECT){
		neededInode++;
	}
	neededInode++;//touch로 만들 새 파일의 Inode
	//printf("%d\n", num_of_free_blocks());
	if(neededInode > num_of_free_blocks()){
		error_message("cpin", local_path, -4);	
		cpintotouch = 1;
	}

	fread(buf, sizeof(buf), 1, fd);

	// copy 시작
	touchFileFail = 0;
	sfs_touch(local_path);
	//파일 만들기 실패
	if(touchFileFail){
		return;
	}
	int n;
	struct sfs_inode newFileInode;
	disk_read(&newFileInode, touchNewFileInode);

	//struct sfs_dir dataBlock[SFS_DENTRYPERBLOCK];
	//bzero(&dataBlock, sizeof(struct sfs_dir));
	//파일이 모두 DIRECT BLOCK에 쓰일 수 있다. 
	if(blockNeeded <= SFS_NDIRECT){
		for(i = 0; i < blockNeeded; i++){
			//더이상 free block이 없어 할당 불가
			if((n = rtn_allocate_free_bitmap(1)) == -1){
				newFileInode.sfi_size += SFS_BLOCKSIZE * i;
				disk_write(&newFileInode, touchNewFileInode);
				fclose(fd);
				return;
			}
			newFileInode.sfi_direct[i] = n;
			disk_write(buf[i], n);
		}
		newFileInode.sfi_size += filesize;
		disk_write(&newFileInode, touchNewFileInode);
		
	}
	//파일이 커서 INDIRECT BLOCK 까지 써야한다
	else{
		//DIRECT BLOCK을 모두 채워준다
		for(i = 0; i < SFS_NDIRECT; i++){
			//더이상 free block이 없어 할당 불가
			if((n = rtn_allocate_free_bitmap(1)) == -1){
				newFileInode.sfi_size += SFS_BLOCKSIZE * i;
				disk_write(&newFileInode, touchNewFileInode);
				//error_message("cpin", local_path, -4);
				fclose(fd);
				return;
			}
			newFileInode.sfi_direct[i] = n;
			disk_write(buf[i], n);
		}

		//나머지를 INDIRECT BLOCK에 채워준다.
		//INDORECT BLOCK을 할당 불가
		if((n = rtn_allocate_free_bitmap(1)) == -1){
			newFileInode.sfi_size += SFS_NDIRECT * SFS_BLOCKSIZE;
			disk_write(&newFileInode, touchNewFileInode);
			fclose(fd);
			return;
		}
		newFileInode.sfi_indirect = n;

		u_int32_t indirectBlock[SFS_DBPERIDB];
		bzero(&indirectBlock, sizeof(indirectBlock));
		//INDIRECT BLOCK에 채워야함
		for( ; i < blockNeeded; i++){
			//INDIRECT BLOCK 에 할당할 INODE가 없음
			if((n = rtn_allocate_free_bitmap(1)) == -1){
				newFileInode.sfi_size += SFS_BLOCKSIZE * i;
				disk_write(&indirectBlock, newFileInode.sfi_indirect);
				disk_write(&newFileInode, touchNewFileInode);
				//error_message("cpin", local_path, -4);
				fclose(fd);
				return;
			}

			indirectBlock[i - SFS_NDIRECT] = n;
			disk_write(buf[i], n);
			
		}
		newFileInode.sfi_size += filesize;
		disk_write(&indirectBlock, newFileInode.sfi_indirect);
		disk_write(&newFileInode, touchNewFileInode);
	}
	fclose(fd);

}

//////////////////////////
// [COMPLETE] sfs_cpout //
//////////////////////////
void sfs_cpout(const char *local_path, const char *path)
{
	int i, j;

	//local_path와 같은 이름이 있는 경우
	u_int32_t fileInode = 0;
	struct sfs_inode curInode;
	disk_read(&curInode, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT;i++){
		if(curInode.sfi_direct[i] == 0)
			continue;
		struct sfs_dir dirBlock[SFS_DENTRYPERBLOCK];
		bzero(&dirBlock, sizeof(dirBlock));
		disk_read(&dirBlock, curInode.sfi_direct[i]);
		for(j = 0; j < SFS_NDIRECT; j++){
			if((dirBlock[j].sfd_ino != 0) &&!strcmp(dirBlock[j].sfd_name, local_path)){
				fileInode = dirBlock[j].sfd_ino;
				break;
			}
		}
		if(fileInode){
			break;
		}
	}
	// 복사하려는 local_path가 없으면 에러
	if(!fileInode){
		error_message("cpout", local_path, -1);
		return;
	}
	
	if(!access(path, 0)){
		error_message("cpout", path, -6);
		return;
	}
	
	FILE* fd;
	if((fd = fopen(path, "wb")) == NULL){
		error_message("cpout", path, -13);
		return;
	}
    chmod(path, 448);

	u_int32_t filesize;

	//copy : local path -> path 
	struct sfs_inode localFile;
	disk_read(&localFile, fileInode);
	filesize = localFile.sfi_size;
	u_int32_t writesize = 0;
	//DIRECT BLOCK 으로 다 끝남.
	if(filesize <= SFS_BLOCKSIZE * SFS_NDIRECT){	
		for(i = 0; i < SFS_NDIRECT; i++){
			char dataBlock[SFS_BLOCKSIZE];
			disk_read(&dataBlock, localFile.sfi_direct[i]);
			if(filesize > SFS_BLOCKSIZE){
				writesize = fwrite(dataBlock, 1, sizeof(dataBlock), fd);
				filesize -= writesize;
			}
			else{
				writesize = fwrite(dataBlock, 1, writesize, fd);
				filesize -= writesize;
			}

			//끝까지 Write 했음
			if(filesize == 0){
				fclose(fd);
				return;
			}
		}
	}

	//INDIRECT BLOCK 까지 써서 카피해야함
	else{

		//DIRECT BLOCK에 있는 데이터 모두 써주기
		for(i = 0; i < SFS_NDIRECT; i++){
			char dataBlock[SFS_BLOCKSIZE];
			disk_read(&dataBlock, localFile.sfi_direct[i]);
			writesize = fwrite(dataBlock, 1, sizeof(dataBlock), fd);
			filesize -= writesize;
		}
		u_int32_t indirectBlock[SFS_DBPERIDB];
		disk_read(indirectBlock, localFile.sfi_indirect);
		for(j = 0; j < SFS_DBPERIDB; j++){
			char dataBlock[SFS_BLOCKSIZE];
			disk_read(&dataBlock, indirectBlock[j]);
			if(filesize > SFS_BLOCKSIZE){
				writesize = fwrite(dataBlock, 1, sizeof(dataBlock), fd);
				filesize -= writesize;
			}
			else{
				writesize = fwrite(dataBlock, 1, filesize, fd);
				filesize -= writesize;
			}
			if(filesize == 0){
				fclose(fd);
				return;
			}			
		}
	}




}

///////////////////////////
// [COMPLETE] dump_inode //
///////////////////////////
void dump_inode(struct sfs_inode inode)
{
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d", inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR)
	{
		for (i = 0; i < SFS_NDIRECT; i++)
		{
			if (inode.sfi_direct[i] == 0)
				break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}
}

///////////////////////////////
// [COMPLETE] dump_directory //
///////////////////////////////
void dump_directory(struct sfs_dir dir_entry[])
{
	int i;
	struct sfs_inode inode;
	for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
	{
		printf("%d %s\n", dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode, dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE)
		{
			printf("\t");
			dump_inode(inode);
			//print_indirect(inode);
		}
	}
}

/////////////////////////
// [COMPLETE] sfs_dump //
/////////////////////////
void sfs_dump()
{
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n", sd_cwd.sfd_ino, sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");
}

/*
 * inode를 넣어주면 해당 inode에 대하여 "ls" 실행
 * inode만 넘어오면 해당 inode의 서브 디렉토리, 파일 출력
 * inode와 name도 넘어오면 inode가 파일인지 디렉토리인지 검사하여 출력
 * sfs_ls 에서 쓰임
 * [COMPLETE] print_list
 */
///////////////////////////
// [COMPLETE] print_list //
///////////////////////////
void print_list(u_int32_t curInodeNum, char name[])
{

	int i, j;
	struct sfs_inode curInode;
	struct sfs_dir dirBlock_entry[SFS_DENTRYPERBLOCK];

	if (name != NULL)
	{
		struct sfs_inode pathInode;
		//read to get subfile TYPE
		disk_read(&pathInode, curInodeNum);
		if (pathInode.sfi_type == SFS_TYPE_FILE)
		{
			printf("%s\n", name);
			return;
		}
	}
	
	disk_read(&curInode, curInodeNum);
	
	for (i = 0; i < SFS_NDIRECT; ++i)
	{
		if (curInode.sfi_direct[i] == 0)
			continue;
		
		disk_read(&dirBlock_entry, curInode.sfi_direct[i]);
		
		//search DIRECTORY BLOCK
		for (j = 0; j < SFS_DENTRYPERBLOCK; ++j)
		{

			if (dirBlock_entry[j].sfd_ino == 0){
				continue;
			}
			struct sfs_inode tempInode;
			//read to get subfile TYPE
			
			disk_read(&tempInode, dirBlock_entry[j].sfd_ino);
			
			//if sub file id dir
			if (tempInode.sfi_type == SFS_TYPE_DIR)
			{
				printf("%s/\t", dirBlock_entry[j].sfd_name);
			}
			//if sub file is file
			else
			{
				printf("%s\t", dirBlock_entry[j].sfd_name);
			}
		}
	}
	printf("\n");
	return;
}

/*
 * 앞에서부터 free block을 찾아 번호를 리턴한다.
 * free block이 없다면 -1을 리턴한다.
 * allocate가 할당되어 있다면 bitmap에 해당 free block을 1로 바꿔 used상태로 바꾼다.
 */
/////////////////////////////////////////
// [COMPLETE] rtn_allocate_free_bitmap //
/////////////////////////////////////////
int rtn_allocate_free_bitmap(int allocate){
	//전체 디스크의 블록 수
	u_int32_t nBitmap = SFS_BITBLOCKS(spb.sp_nblocks);

	int i, j, k;
	char bitmapBlock[SFS_BLOCKSIZE];
	for(i = 0; i < nBitmap; i++){
		bzero(&bitmapBlock, sizeof(bitmapBlock));
		disk_read(&bitmapBlock, 2 + i);
		for(j = 0; j < SFS_BLOCKSIZE; j++){
			for(k = 0; k < CHAR_BIT; k++){
				int numberOfBlock = CHAR_BIT * SFS_BLOCKSIZE * i + CHAR_BIT * j + k;
				//free block이고 갯수가 spb.sp_nblocks개 이하여야 성립.
				if(!BIT_CHECK(bitmapBlock[j], k) && (numberOfBlock <= spb.sp_nblocks -1)){
					if(allocate == 1){
						BIT_SET(bitmapBlock[j], k);
						disk_write(&bitmapBlock, 2+i);
					}
					return numberOfBlock;
				}

			}
		}
	}
	return -1;
}

/*
 * 삭제하고 싶은 i-node를 받는다.
 * 해당 i-node의 Direct ptr를 모두 삭제해준다. 빈 파일임은 메인에서 커버한다.
 * Direct ptr을 모두 삭제하고 원래 삭제하고자 했던 i-node를 삭제해준다.
 */
//////////////////////////////////////
// [COMPLETE] make_free_blockNumber //
//////////////////////////////////////
void make_free_blockNumber(u_int32_t freeNumber, int blockNumber){
	int i;
	u_int32_t subFreeNumber;
	u_int32_t bitmapBlockNumber;
	u_int32_t charLocation;
	char bitmap[SFS_BLOCKSIZE]; //읽어올 비트맵 512바이트
	struct sfs_inode deleteInode;
	disk_read(&deleteInode, freeNumber);
	//해당 BLOCK NUMBER만 삭제하는경우
	if(blockNumber){
		subFreeNumber = freeNumber;
		bitmapBlockNumber = 2 + SFS_BITBLOCKS(subFreeNumber)-1;
		subFreeNumber -= (bitmapBlockNumber-2)*(SFS_BLOCKBITS);
		charLocation = subFreeNumber % CHAR_BIT;
		subFreeNumber /= CHAR_BIT;
		disk_read(&bitmap, bitmapBlockNumber);
		BIT_CLEAR(bitmap[subFreeNumber], charLocation);
		disk_write(&bitmap, bitmapBlockNumber);
		return;
	}

	//지우려는게 I-node라 i-node 번호 및 하위 파일들 삭제.

	for(i = 0; i < SFS_NDIRECT; i++){
		if(deleteInode.sfi_direct[i] == 0){
			continue;
		}
		//삭제할 i-node번호가 위치한 블록번호를 구한다.

		subFreeNumber = deleteInode.sfi_direct[i];
		bitmapBlockNumber = 2 + SFS_BITBLOCKS(subFreeNumber) -1;
		subFreeNumber -= (bitmapBlockNumber-2)*(SFS_BLOCKBITS);
		charLocation = subFreeNumber % CHAR_BIT;
		subFreeNumber /= CHAR_BIT;
		
		//이쯤에 sfi_direct = 0 을 해줘야하나????????????@@@@@@@@@@@@@@@@@@@@
		disk_read(&bitmap, bitmapBlockNumber);
		BIT_CLEAR(bitmap[subFreeNumber], charLocation);
		disk_write(&bitmap, bitmapBlockNumber);
	}

	subFreeNumber = freeNumber;
	bitmapBlockNumber = 2 + SFS_BITBLOCKS(subFreeNumber)-1;
	subFreeNumber -= (bitmapBlockNumber-2)*(SFS_BLOCKBITS);
	charLocation = subFreeNumber % CHAR_BIT;
	subFreeNumber /= CHAR_BIT;
	disk_read(&bitmap, bitmapBlockNumber);
	BIT_CLEAR(bitmap[subFreeNumber], charLocation);
	disk_write(&bitmap, bitmapBlockNumber);
	
}

/*
 * 삭제하고 싶은 i-node를 받는다.
 * 해당 i-node의 Direct ptr를 모두 삭제해준다. 빈 파일임은 메인에서 커버한다.
 * Direct ptr을 모두 삭제하고 원래 삭제하고자 했던 i-node를 삭제해준다.
 */
//////////////////////////////////////
/// [COMPLETE] num_of_free_blocks ////
//////////////////////////////////////
u_int32_t num_of_free_blocks(){
	//전체 디스크의 블록 수
	u_int32_t nBitmap = SFS_BITBLOCKS(spb.sp_nblocks);
	u_int32_t count = 0;
	int i, j, k;
	char bitmapBlock[SFS_BLOCKSIZE];
	for(i = 0; i < nBitmap; i++){
		bzero(&bitmapBlock, sizeof(bitmapBlock));
		disk_read(&bitmapBlock, 2 + i);
		for(j = 0; j < SFS_BLOCKSIZE; j++){
			for(k = 0; k < CHAR_BIT; k++){
				//free block이고 갯수가 spb.sp_nblocks개 이하여야 성립.
				int numberOfBlock = SFS_BLOCKSIZE*CHAR_BIT*i + CHAR_BIT*j + k;
				if(!BIT_CHECK(bitmapBlock[j], k) && (numberOfBlock <= spb.sp_nblocks -1)){
					count++;
				}

			}
		}
	}
	return count;
}

//////////////////////////////////////
/// [COMPLETE] getfilesize ///////////
//////////////////////////////////////
u_int32_t getfilesize(const char* path){
	FILE* fd;
	fd = fopen(path, "rb");
	u_int32_t filesize;
	fseek(fd, 0, SEEK_END);
	filesize = ftell(fd);
	fclose(fd);
	return filesize;
}

//////////////////////////////////////
/// [COMPLETE] print_indirect ////////
//////////////////////////////////////
void print_indirect(struct sfs_inode inode){
	if(inode.sfi_type == SFS_TYPE_FILE){
		u_int32_t indirectBlock[SFS_DBPERIDB];
		disk_read(indirectBlock, inode.sfi_indirect);
		int i;
		for(i = 0; i < SFS_DBPERIDB; i++){
			printf("%d\t", indirectBlock[i]);
		}
		printf("\n\n");
	}
}

