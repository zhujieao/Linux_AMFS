#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main (int argc, char **argv)
{
    int lflag = 0;
    int aflag = 0;
    int rflag = 0;
    int otherflag = 0;
    char *avalue = NULL;
    char *rvalue = NULL;
    char *path = NULL;
    int index;
    int fd;
    int ret;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "la:r:")) != -1)
        switch (c)
        {
        case 'l':
            lflag = 1;
            break;
        case 'a':
            aflag = 1;
            avalue = optarg;
            break;
        case 'r':
            rflag = 1;
            rvalue = optarg;
            break;
        case '?':
            if (optopt == 'a' || optopt == 'r')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                         "Unknown option character `\\x%x'.\n",
                         optopt);
            return 1;
        default:
            otherflag =1;
            abort ();
        }
    //printf ("lflag = %d, aflag = %d, rflag = %d, avalue = %s, rvalue = %s\n",lflag, aflag, rflag, avalue, rvalue);

    index = optind; 
    if(index < argc){
        //printf ("Non-option argument %s\n", argv[index]);
        path = argv[index];
    }else{
        printf("missing mount path\n");
        return -1;
    }

    fd = open(path, O_RDONLY);
    if(fd < 0){
        printf("failed getting access to mounted path\n");
        close(fd);
        return 1;
    }

    if(lflag && (!aflag) && (!rflag) && (!otherflag)) {
        int *len;
        char * data;
        int i = 0;
        ret = ioctl(fd, _IOR('x', 2 , char *), (unsigned long)len);
        printf("amfsctl:returned length:%d\n", *len);
        data = malloc(sizeof(char) * (*len));
        ret = ioctl(fd, _IOR('x', 1 , char *), (unsigned long)data);
        while(i < *len){
            printf("%c", data[i]);
            i ++;
        }
    }else if((!lflag) && aflag && (!rflag) && (!otherflag)) {
        ret = ioctl(fd, _IOR('x', 3 , char *), (unsigned long)avalue);
        printf("amfsctl:returned:%s\n", avalue);
    }else if((!lflag) && (!aflag) && rflag && (!otherflag)) {
        ret = ioctl(fd, _IOR('x', 4 , char *), (unsigned long)rvalue);
        printf("amfsctl:returned:%s\n", rvalue);
    }else{
        printf("sample help documentation.\n");
    }
    close(fd);
    return ret;
}
