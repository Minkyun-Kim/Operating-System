mysfs:	sfs_disk.o sfs_func_hw.o sfs_main.o sfs_func_ext.o
	cp ../img/* .
	cc -o mysfs sfs_func_hw.o sfs_disk.o sfs_main.o sfs_func_ext.o

sfs_disk.o:	sfs_types.h sfs_disk.h sfs_disk.c
	cc -c -o sfs_disk.o sfs_disk.c

sfs_func_hw.o:	sfs_types.h sfs_func.h sfs_disk.h sfs.h sfs_func_hw.c
	cc -c -o sfs_func_hw.o sfs_func_hw.c

sfs_main.o:	sfs_func.h sfs_main.c
	cc -c -o sfs_main.o sfs_main.c

sfs_func_ext.o:	

clear:
	rm sfs_disk.o
	rm sfs_func_hw.o
	rm sfs_main.o
