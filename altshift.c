#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <wordexp.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>

typedef long long longlong;

enum {
	s_initial,
	s_onekey_1,
	s_twokeys,
	s_onekey_2,
	s_jam
};

static int numPressed=0;

struct keys {
	int key1;
	int key2;
	int state;
};

static int processEvent(struct keys *keys, unsigned short code, int value)
{
	int res=0;
	if (numPressed<1) {
		res=(keys->state==s_onekey_2);
		keys->state=s_initial;
	} else if (numPressed==1) {
		if (keys->state==s_initial && value && (code==keys->key1 || code==keys->key2)) {
			keys->state=s_onekey_1;
		} else if (keys->state==s_twokeys && !value && (code==keys->key1 || code==keys->key2)) {
			keys->state=s_onekey_2;
		} else {
			keys->state=s_jam;
		}
	} else if (numPressed==2) {
		keys->state=(keys->state==s_onekey_1 && value && (code==keys->key1 || code==keys->key2)) ?
			s_twokeys : s_jam;
	} else {
		keys->state=s_jam;
	}
	return res;
}

static void help(char *argv0, int status)
{
	FILE *fo=status ? stderr : stdout;
	fprintf(fo,
		"Usage: %s [OPTION]...\n"
		"Usage: %s -h\n"
		"Monitor keyboard input events and switch keyboard layout using external commands.\n"
		"  [if=file]                   input event device file, defaults to stdin\n"
		"  [langKeys=<key1>,<key2>]    key combination to switch keyboard layout\n"
		"                              default 56,42 (left ALT + left SHIFT)\n"
		"  [variantKeys=<key1>,<key2>] key combination to switch keyboard variant\n"
		"                              default 29,42 (left CTRL + left SHIFT)\n"
		"  [cmdDef=cmd]                command to switch to default layout\n"
		"                              defaults to 'setxkbmap \"us\"'\n"
		"  [cmdAlt=cmd]                command to switch to alternate layout\n"
		"                              defaults to 'setxkbmap \"bg,us\" \"phonetic,\"'\n"
		"  [cmdAltVariant=cmd]         command to switch to alternate variant\n"
		"                              defaults to 'setxkbmap \"bg,us\" \",\"'\n"
		"                              use empty string to disable alternate variant\n"
		"Examples:\n"
		"  %s if=/dev/input/event0 cmdDev='echo D' cmdAlt='echo A' cmdAltVariant='echo A2'\n"
		"  (produce debug output)\n" 
		"  %s if=/dev/input/event0 cmdAlt='setxkbmap \"bg,us\"' cmdAltVariant=\n"
		"  (use Bulgarian BDS layout, no alternate variant)\n" 
		,
		argv0, argv0, argv0, argv0
	);
	exit(status);
}

static void exitFailedCmd(char *cmd)
{
	fprintf(stderr, "error executing command: %s\n", cmd);
	exit(10);
}

static void forkExecWait(char *cmd)
{
	int pid=fork();
	if (pid) {
		int wstatus=0;
		int wpid=waitpid(pid, &wstatus, 0);
		if (pid!=wpid) {
			fprintf(stderr, "waitpid failed, expected %lld, got %lld\n",
				(longlong)pid, (longlong)wpid);
			exitFailedCmd(cmd);
		}
		if (!WIFEXITED(wstatus)) {
			fprintf(stderr, "abnormal command termination\n");
			exitFailedCmd(cmd);
		}
		int status=WEXITSTATUS(wstatus);
		if (status) {
			fprintf(stderr, "command failed, status %d\n", status);
			exitFailedCmd(cmd);
		}
	} else {
		wordexp_t p;
		int res=wordexp(cmd, &p, 0);
		if (res) {
			fprintf(stderr, "wordexp failed (posix-shell like word expansion): %d\n", res);
			exit(7);
		}
		if (p.we_wordc>=1) {
			char **cmds=malloc((p.we_wordc+1)*sizeof(char *));
			for (int i=0; i<p.we_wordc; i++)
				cmds[i]=strdup(p.we_wordv[i]);
			cmds[p.we_wordc]=NULL;
			execvp(cmds[0], cmds);
			fprintf(stderr, "execvp() failed (%d):%s\n", errno, strerror(errno));
			// free cmds, wordfree
			exitFailedCmd(cmd);
		}
		//wordfree(&p);
		fprintf(stderr, "wordexp command expansion empty\n");
		exitFailedCmd(cmd);
	}
}

int main(int argc, char *argv[])
{
	char *fname=NULL;
	struct keys langKeys, variantKeys;
	langKeys.state=s_initial;
	langKeys.key1=KEY_LEFTALT;
	langKeys.key2=KEY_LEFTSHIFT;
	variantKeys.state=s_initial;
	variantKeys.key1=KEY_LEFTCTRL;
	variantKeys.key2=KEY_LEFTSHIFT;
	if (argc>1 && !strcmp(argv[1], "-h"))
		help(argv[0], 0);
	char *cmds[3]={
		"setxkbmap \"us\"",
		"setxkbmap \"bg,us\" \"phonetic,\"",
		"setxkbmap \"bg,us\" \",\"",
	};
	for (int i=1; i<argc; i++) {
		char *arg=argv[i];
		if (!strncmp(arg, "if=", 3)) {
			fname=arg+3;
		} else if (!strncmp(arg, "langKeys=", 9)) {
			if (2!=sscanf(arg+9, "%d,%d", &langKeys.key1, &langKeys.key2))
				help(argv[0], 1);
		} else if (!strncmp(arg, "variantKeys=", 12)) {
			if (2!=sscanf(arg+12, "%d,%d", &variantKeys.key1, &variantKeys.key2))
				help(argv[0], 1);
		} else if (!strncmp(arg, "cmdDef=", 7)) {
			cmds[0]=arg+7;
		} else if (!strncmp(arg, "cmdAlt=", 7)) {
			cmds[1]=arg+7;
		} else if (!strncmp(arg, "cmdAltVariant=", 14)) {
			cmds[2]=arg+14;
		} else {
			help(argv[0], 1);
		}
	}
	int inpfd=0;
	if (fname) {
		inpfd=open(fname, O_RDONLY);
		if (inpfd<0) {
			fprintf(stderr, "Could not open input event device file '%s'\n", fname);
			return 2;
		}
	}
	int lang=0, variant=0;
	int doFork=0;
	while (1) {
		struct input_event ev;
		ssize_t nrd=read(inpfd, &ev, sizeof(ev));
		if (nrd!=sizeof(ev)) {
			fprintf(stderr, "Reading input event device failed (expected %lld, got %lld)\n",
				(longlong)sizeof(ev), (longlong)nrd);
			return 3;
		}
		if (ev.type==EV_KEY && (ev.value==0 || ev.value==1)) {
			if (ev.value) {
				++numPressed;
			} else {
				if (--numPressed<0)
					numPressed=0;
			}
			doFork=0;
			if (processEvent(&langKeys, ev.code, ev.value)) {
				lang=!lang;
				doFork=1;
			}
			if (cmds[2][0] && processEvent(&variantKeys, ev.code, ev.value)) {
				variant=!variant;
				doFork=lang;
			}
			if (doFork) {
				int cmdIdx=lang;
				if (cmdIdx)
					cmdIdx+=variant;
				forkExecWait(cmds[cmdIdx]);
			}
		}
	}
}
