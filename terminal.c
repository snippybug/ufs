#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <mcheck.h>

#include "fsinfo.h"
#include "file.h"
#include "common.h"

int mon_help(int argc, char **argv);
int mon_exit(int argc, char **argv);
int mon_ls(int argc, char **argv);
int mon_create(int argc, char **argv);
int mon_mkdir(int argc, char **argv);
int mon_cd(int argc, char **argv);
int mon_rmdir(int argc, char **argv);
int mon_rm(int argc, char **argv);
int mon_read(int argc, char **argv);
int mon_write(int argc, char **argv);

#define BUFLEN 1024
#define WHITESPACE "\t\r\n "
#define MAXARGS 16
#define MAX_PASS_LEN 8

static char buf[BUFLEN];
struct Command{
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char **argv);
};

static struct Command commands[]={
	{ "help", "Display this list of commands", mon_help },
	{ "exit", "Exit the monitor", mon_exit },
	{ "ls", "List the current directory", mon_ls},
	{ "create", "Create a file", mon_create},
	{ "mkdir", "Create a directory", mon_mkdir},
	{ "cd", "Change working directory", mon_cd },
	{ "rmdir", "Remove an empty directory", mon_rmdir },
	{ "rm", "Remove a file", mon_rm },
	{ "read", "Read some bytes from the file", mon_read },
	{ "write", "Write some bytes to the file", mon_write }

};

#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))


/* Originate from APUE 3rd */
char*
getpass(const char *prompt){
	static char buf[MAX_PASS_LEN + 1];
	char *ptr;
	sigset_t sig, osig;
	struct termios ts, ots;
	FILE *fp;
	int c;

	if((fp = fopen(ctermid(NULL), "r+")) == NULL)
		return NULL;
	setbuf(fp, NULL);

	sigemptyset(&sig);
	sigaddset(&sig, SIGINT);		// block SIGINT
	sigaddset(&sig, SIGTSTP);		// block SIGTSTP
	sigprocmask(SIG_BLOCK, &sig, &osig);	// and save mask

	tcgetattr(fileno(fp), &ts);		// save tty state
	ots = ts;				// structure copy
	ts.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	ts.c_lflag &= ~ICANON;			// 非规范模式：输入数据不装配成行
	ts.c_cc[VMIN] = 1;
	ts.c_cc[VTIME] = 0;
	tcsetattr(fileno(fp), TCSAFLUSH, &ts);
	fputs(prompt, fp);

	ptr = buf;
	while((c = getc(fp)) != EOF && c != '\n'){
		if(c == '\b' || c == '\x7f'){
			if(ptr != buf){
				putc('\b', fp);
				putc(' ', fp);		// 看起来像刚才的输入被删除了
				putc('\b', fp);
				ptr--;
			}
		}else if(ptr < &buf[MAX_PASS_LEN]){
			putc('*', fp);
			*ptr++ = c;
		}
	}
	*ptr = 0;				// null terminate
	putc('\n', fp);				// we echo a newline

	tcsetattr(fileno(fp), TCSAFLUSH, &ots);		// restore TTY state 
	sigprocmask(SIG_SETMASK, &osig, NULL);		// restore mask
	fclose(fp);				// done with /dev/tty
	return buf;
}

char*
readline(const char *prompt){
	int i, c, echoing;
	
	if(prompt != NULL)
		fprintf(stderr, "%s", prompt);

	i=0;
	while(1){
		c = getchar();
		if(c == EOF){
			return NULL;
		}else if(c < 0){
			fprintf(stderr, "read error: %s\n", strerror(errno));
			return NULL;
		}else if((c == '\b' || c == '\x7f') && i > 0){
			fputc('\b', stderr);
			i--;
		}else if(c >= ' ' && i < BUFLEN-1){
			fputc(c, stderr);
			buf[i++] = c;
		}else if(c == '\n' || c == '\r'){
			fputc('\n', stderr);
			buf[i] = 0;
			return buf;
		}
	}
}

static int
runcmd(char *buf){
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while(1){
		// gobble whitespace
		while(*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if(*buf == 0)
			break;

		// save and scan past next arg
		if(argc == MAXARGS - 1){
			fprintf(stderr, "Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while(*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// look up and invoke the command
	if(argc == 0)
		return 0;
	for(i=0; i<NCOMMANDS; i++){
		if(strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv);
	}
	printf("Unknown command '%s'\n", argv[0]);
	return 0;
}

int
main(int argc, char **argv){
	char *buf;
	int r;
	char *infile=NULL;
	FILE *fp=NULL;

	setenv("MALLOC_TRACE", "meminfo", 1);
	mtrace();

	while(--argc){
		argv++;
		if(**argv == '?'){
		printf("Version 1.4 (C) Wang Zonglei\n"
		"A terminal to communicate with the UFS filesystem\n"
		"Usage:\n"
		"\tterminal [-input <file>]\n");
		return 0;
		}
		if(**argv == '-'){
			if(strcmp(++*argv, "input") == 0){
				infile = *(++argv);
				--argc;
				if(infile == NULL){
					fprintf(stderr, "Error: an input file is needed\n");
					return -1;
				}
			}else{
				fprintf(stderr, "Error: unknown argument %s\n", *argv);
				return -1;
			}
		}
	}
	if(infile){
		if((fp=freopen(infile, "r", stdin)) == NULL){
			fprintf(stderr, "Error: can't redirect the stdin to file %s\n", infile);
		return -1;
		}
	}
	if(initfs() < 0){
		printf("Can't initialize the FS, Please check and retry\n");
		return -1;
	}

//	buf = readline("User name: ");
//	buf = getpass("Password: ");

	printf("Welcome to the UFS file system monitor!\n");
	printf("Type 'help' for a list of commands.\n");
	printf("Type 'exit' to exit this monitor.\n");
	while(1){
		buf = readline("K> ");
		if(buf != NULL){
			if(runcmd(buf) < 0){
				break;
			}
		}else{
			break;
		}
	}
	if(fp){
		fclose(fp);
	}
	return 0;
}

int mon_help(int argc, char **argv){
	int i;

	for(i=0;i<NCOMMANDS;i++){
		printf("%d. %s - %s\n", i, commands[i].name, commands[i].desc);
	}
	return 0;
}

int mon_exit(int argc, char **argv){
	return -1;
}

int 
mon_ls(int argc, char **argv){
	char *path;
	if(argc == 1){
		path = NULL;
	}else{
		path = argv[1];
	}
	struct ufs_entry_info *entry = readdir(path);
	if(entry == NULL){
		fputc('\n', stderr);
		return 0;
	}
	struct ufs_entry_info *temp;
	while(entry){
		if(entry->file_type == FDIR)	// 目录项在名字后面加上/
			fprintf(stderr, "%s/ ", entry->name);
		else
			fprintf(stderr, "%s ", entry->name);
		temp = entry;
		entry = entry->next;
		free(temp);			// readdir只负责分配
	}
	fputc('\n', stderr);
	return 0;
}

int mon_create(int argc, char **argv){
	assert(argc == 2);

	ufs_create(argv[1], FREG);
	return 0;
}

int mon_mkdir(int argc, char **argv){
	assert(argc == 2);

	ufs_create(argv[1], FDIR);
	return 0;
}

int mon_cd(int argc, char **argv){
	assert(argc == 2);
	
	ufs_cd(argv[1]);
	return 0;
}

int mon_rm(int argc, char **argv){
	assert(argc == 2);
	ufs_rm(argv[1], FREG);
	return 0;
}

int mon_rmdir(int argc, char **argv){
	assert(argc == 2);
	ufs_rm(argv[1], FDIR);
	return 0;
}

int mon_read(int argc, char **argv){
	char buf[256];
	assert(argc == 3);
	int n = atoi(argv[2]);
	assert(n < 255);
	ufs_read(argv[1], n, buf);
	buf[n] = 0;
	printf("read: %s\n", buf);
	return 0;
}

int mon_write(int argc, char **argv){
	assert(argc == 3);
	ufs_write(argv[1], strlen(argv[2]), argv[2]);
	return 0;
}
