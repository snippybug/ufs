#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <termios.h>

int mon_help(int argc, char **argv);
int mon_exit(int argc, char **argv);

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
	{ "exit", "Exit the monitor", mon_exit }
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
		printf("%s", prompt);

	i=0;
	while(1){
		c = getchar();
		if(c < 0){
			fprintf(stderr, "read error: %s\n", strerror(errno));
			return NULL;
		}else if((c == '\b' || c == '\x7f') && i > 0){
		//	putchar('\b');
			i--;
		}else if(c >= ' ' && i < BUFLEN-1){
		//	putchar(c);
			buf[i++] = c;
		}else if(c == '\n' || c == '\r'){
		//	putchar('\n');
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

int main(){
	char *buf;
	int r;

	buf = readline("User name: ");
	buf = getpass("Password: ");
	printf("passwd=%s\n", buf);

	printf("Welcome to the UFS file system monitor!\n");
	printf("Type 'help' for a list of commands.\n");
	printf("Type 'exit' to exit this monitor.\n");
	while(1){
		buf = readline("K> ");
		if(buf != NULL)
			if(runcmd(buf) < 0)
				break;
	}
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
