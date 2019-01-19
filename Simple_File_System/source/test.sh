cc -o mysfs sfs_disk.c sfs_func_hw.c sfs_main.c sfs_func_ext.o 
cp ../img/* .
mysfs < $1 > my.txt
rm ok12sfs
cp ../img/* .
sfs < $1 > ans.txt
rm ok12sfs
diff my.txt ans.txt
cp ../img/* .
