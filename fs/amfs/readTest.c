#include <stdio.h>
#include <stdlib.h>
 
int main()
{
    FILE *fp;
    char ch;
    fp = fopen("/mnt/amfs/hw1/uuu","w");
    if (fp == NULL) {
        printf("/mnt/amfs/ttt cannot open file\n");
        exit(1);
    }
    printf("putch reault: %d\n",fputc('1',fp));
    printf("putch reault: %d\n",fputc('v',fp));
    printf("putch reault: %d\n",fputc('i',fp));
    printf("putch reault: %d\n",fputc('r',fp));
    printf("putch reault: %d\n",fputc('u',fp));
    printf("putch reault: %d\n",fputc('s',fp));
    printf("putch reault: %d\n",fputc('c',fp));
    printf("putch reault: %d\n",fputc('c',fp));
    ch = fgetc(fp);
    printf("%c",ch);
    fclose(fp);
    return 0;
}
