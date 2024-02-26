#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h> // 包含头文件 <unistd.h>
int main() {
char *file_path1 = "/home/gyx/Desktop/copy/test.txt";
char *file_path2 ="/home/gyx/Desktop/copy/dest.txt";
FILE *fp1,*fp2;
fp1= fopen(file_path1, "r");
if (fp1 == NULL) {
perror("Error in opening file");
exit(EXIT_FAILURE);
} else {
printf("File opened successfully 1 \n");
// 在此处可以执行其他操作，比如读取文件内容或者进行其他处理。
char tmp[10000];
while(fgets(tmp,sizeof(tmp),fp1)!=NULL)
printf("%s",tmp);
}
printf("\n");
fp2= fopen(file_path2, "r");
if (fp2 == NULL) {
perror("Error in opening file");
exit(EXIT_FAILURE);
} else {
printf("File opened successfully 2 \n");
// 在此处可以执行其他操作，比如读取文件内容或者进行其他处理。
char tmp[10000];
while(fgets(tmp,sizeof(tmp),fp2)!=NULL)
printf("%s",tmp);
}
printf("PID==>%d\n",getpid());
getchar();
fclose(fp1); // 关闭文件
fclose(fp2); // 关闭文件
//while(1);
return 0;
}