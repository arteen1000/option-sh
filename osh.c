#include <getopt.h> // use getopt_long, rely on fiddling with optind
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "my-lib/stoi-ge0.h"

#define RDONLY_LONG_INDEX 11
#define WRONLY_LONG_INDEX 12
#define RDWR_LONG_INDEX 13
#define PIPE_LONG_INDEX 14
#define CMD_LONG_INDEX 15

#define STARTING_DESCRIPTORS 32
#define STARTING_COMMANDS 16

#if ! defined O_RSYNC // macOS

#define O_RSYNC 0 // make it a no op

#endif

// parent ensures responsibility of making sure everything correct on it's side, and then fork the child and move on
// always exec seq. and stop at first failure to ensure no data damage and adverse effects (ex., if file_nums are all off because a file couldn't be opened)

// refuse to leak pipe file descriptors into children -- pipes always CLOEXEC (duplicated fd's are not by default), '--close file_num' pipes in parent (option-sh) responsibility of caller

// can also add option to automatically close pipe ends in parent later, would only require a little extra engineering

int next_fd_swap[3];

struct fd_list_struct
{
  size_t fd_count;
  size_t max_fd_count;
  int * fds;
} fd_list;

struct cmd_struct
{
  size_t argv_start_index;
  size_t argv_end_index;
  int child_pid;  
};

struct cmd_list_struct
{
  size_t cmd_count;
  size_t max_cmd_count;
  struct cmd_struct * cmds;
} cmd_list;

// ensure that there is enough space for additional fds, if not -- double the space (eventually hits system limit)
void
check_fds(void)
{
  if (fd_list.fd_count == fd_list.max_fd_count)
	{
	  fd_list.fds = realloc(fd_list.fds, sizeof *fd_list.fds * fd_list.max_fd_count * 2);
	  if (fd_list.fds == NULL)
		{
		  perror("realloc");
		  exit(errno);
		}
	  fd_list.max_fd_count = fd_list.max_fd_count * 2;
	}
}

// handle initial command allocation as well, this is done prior to filling the command
void
check_cmds(void)
{
  if (cmd_list.max_cmd_count == 0)
	{
	  cmd_list.cmds = malloc(sizeof *cmd_list.cmds * STARTING_COMMANDS);
	  if (cmd_list.cmds == NULL)
		{
		  perror("malloc");
		  exit(errno);
		}
	  cmd_list.max_cmd_count = STARTING_COMMANDS;
	}
  else if (cmd_list.cmd_count == cmd_list.max_cmd_count)
	{
	  cmd_list.cmds = realloc(cmd_list.cmds, sizeof *cmd_list.cmds * cmd_list.max_cmd_count * 2);
	  if (cmd_list.cmds == NULL)
		{
		  perror("realloc");
		  exit(errno);
		}
	  cmd_list.max_cmd_count = cmd_list.max_cmd_count * 2;
	}
}

// handle too few arguments to '--command'
void
handle_too_few_args(char * argv[], int i)
{
  if (i < 4)
	{
	  fprintf(stderr, "%s: %d too few arguments to '--command", argv[0], 4 - i);
	  for (; i > 0; i--)
		fprintf(stderr, " %s", argv[optind - i]);
	  fputs("'\n", stderr);
	  exit(EXIT_FAILURE);
	}
}

// convert the user file number to a file descriptor
int
file_num_to_fd(int file_num, char const * prog)
{
  if (file_num >= fd_list.fd_count)
	{
	  fprintf(stderr,"%s: invalid file num '%d' to '--command'\n", prog, file_num);
	  exit(EXIT_FAILURE);
	}
  return fd_list.fds[file_num];
}

void
handle_dups(void)
{
  if (-1 == dup2(next_fd_swap[0], STDIN_FILENO))
	{
	  perror("dup2");
	  exit(errno);
	}
  if (-1 == dup2(next_fd_swap[1], STDOUT_FILENO))
	{
	  perror("dup2");
	  exit(errno);
	}
  if (-1 == dup2(next_fd_swap[2], STDERR_FILENO))
	{
	  perror("dup2");
	  exit(errno);
	}  
}
  
void
handle_redirections(char const * str, int i, char *argv[])
{
  int fd_to_swap_in = -1;
  
  if (str[1] == '\0')
	switch (str[0])
	  {
	  case 'i':
		fd_to_swap_in = STDIN_FILENO;
		break;
	  case 'o':
		fd_to_swap_in = STDOUT_FILENO;
		break;
	  case 'e':
		fd_to_swap_in = STDERR_FILENO;
		break;
	  }
  if (fd_to_swap_in == -1)
	{
	  int file_num = stoi_ge0(str);
	  if (file_num == -1)
		{
		  fprintf(stderr, "%s: bad argument '%s' to '--command", argv[0], str);
		  for (; i > 0; i--)
			fprintf(stderr, " %s", argv[optind - i]);
		  fputs("'\n", stderr);
		  exit(EXIT_FAILURE);
		}
	  fd_to_swap_in = file_num_to_fd(file_num, argv[0]);
	}
  next_fd_swap[i] = fd_to_swap_in;
}

void
exec_cmd(char * argv[])
{
  cmd_list.cmds[cmd_list.cmd_count].argv_end_index = optind - 1;
  pid_t child_pid = fork();
  if (-1 == child_pid)
	{
	  perror("fork");
	  exit(errno);
	}
  else if (0 == child_pid)
	{
	  handle_dups();
	  size_t command_name_index = cmd_list.cmds[cmd_list.cmd_count].argv_start_index;

	  // set argv to be NULL one past end of argument list for command
	  argv[cmd_list.cmds[cmd_list.cmd_count].argv_end_index + 1] = NULL;
	  if (strcmp(argv[command_name_index], "./osh") == 0) exit(5);
	  execvp(argv[command_name_index], argv + command_name_index);
	  // handle bad execvp
	  const char * execvp_error = strerror(errno);
	  fprintf(stderr, "%s: 'execvp' failed with message '%s' for '--command", argv[0], execvp_error);
	  for (size_t i = command_name_index - 3; i < cmd_list.cmds[cmd_list.cmd_count].argv_end_index + 1; i++)
		fprintf(stderr, " %s", argv[i]);
	  fputs("'\n", stderr);
	  exit(EXIT_FAILURE);
	}
  else
	{
	  cmd_list.cmds[cmd_list.cmd_count].child_pid = child_pid;
	  cmd_list.cmd_count++;
	}
}

int
main (int argc, char * argv[])
{
  int optc;
  int long_index;
  
  int next_oflags = 0;
  int oflag = 0;

  fd_list.fds = malloc(sizeof *fd_list.fds * STARTING_DESCRIPTORS);
  if (fd_list.fds == NULL)
	{
	  perror("malloc");
	  exit(errno);
	}
  fd_list.max_fd_count = STARTING_DESCRIPTORS;
  
  struct option const long_options[] =
	{
	  { "append", no_argument, &oflag, O_APPEND },
	  { "cloexec", no_argument, &oflag, O_CLOEXEC },
	  { "creat", no_argument, &oflag, O_CREAT },
	  { "directory", no_argument, &oflag, O_DIRECTORY },
	  { "dsync", no_argument, &oflag, O_DSYNC },
	  { "excl", no_argument, &oflag, O_EXCL },
	  { "nofollow", no_argument, &oflag, O_NOFOLLOW },
	  { "nonblock", no_argument, &oflag, O_NONBLOCK },	  
	  { "rsync", no_argument, &oflag, O_RSYNC },
	  { "sync", no_argument, &oflag, O_SYNC },
	  { "trunc", no_argument, &oflag, O_TRUNC },
	  
	  { "rdonly", required_argument, &oflag, O_RDONLY },
	  { "wronly", required_argument, &oflag, O_WRONLY },
	  { "rdwr", required_argument, &oflag, O_RDWR },
	  
	  { "pipe", no_argument, NULL, 0 },
	  
	  { "command", no_argument, NULL, 0 }, // treat args as non-option args
	  { "wait", no_argument, NULL, 0 },
	  
	  { "chdir", required_argument, NULL, 0 },
	  { "close", required_argument, NULL, 0 },
	  { "verbose", no_argument, NULL, 0 },
	  { "profile", no_argument, NULL, 0 },
	  { "abort", no_argument, NULL, 0 },
	  { "catch", required_argument, NULL, 0 },
	  { "ignore", required_argument, NULL, 0 },
	  { "default", required_argument, NULL, 0 },
	  { "pause", no_argument, NULL, 0 },
	  { NULL, 0, NULL, 0 },
	};

	do
	  {
		optc = getopt_long(argc, argv, "-:", long_options, &long_index);
		if (optc == -1)
		  break;

		switch (optc)
		  {
		  case '?':
			fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[optind - 1]);
			exit(EXIT_FAILURE);
		  case ':':
			fprintf(stderr, "%s: missing arg for '%s'\n", argv[0], argv[optind - 1]);
			exit(EXIT_FAILURE);
		  case 1:
			// should never hit with way optind handled, relies on getopt implementation sharing global state w/ us
			fprintf(stderr, "%s: non-option arg '%s' not allowed\n", argv[0], optarg);
			break;
			exit(EXIT_FAILURE);
		  case 0:
			// all long options, long_options[long_index], optarg if arg
			// also, always check optind < argc if manipulating

			// handles all the open options
			next_oflags |= oflag;

			switch (long_index)
			  {
			  case RDONLY_LONG_INDEX:
			  case WRONLY_LONG_INDEX:
			  case RDWR_LONG_INDEX:
				{
				  int fd = open(optarg, next_oflags, 0644);
				  if (fd == -1)
					{
					  const char * open_error = strerror(errno);
					  fprintf(stderr, "%s: 'open' failed with message '%s' for '--%s %s'\n", argv[0], open_error, long_options[long_index].name, optarg);
					  exit(EXIT_FAILURE);
					}
				  fd_list.fds[fd_list.fd_count] = fd;
				  next_oflags = 0;
				  fd_list.fd_count++;
				}
				check_fds();
				break;
			  case CMD_LONG_INDEX:
				{
				  check_cmds();
				  // starts at the first (input re-direction) and goes one past last arg to command
				  for (int i = 0; ; optind++, i++)
					{
					  if (argv[optind] == NULL)
						{
						  handle_too_few_args(argv, i);
						  exec_cmd(argv);
						  break;
						}
					  else if (argv[optind][0] == '-' && argv[optind][1] == '-')
						{
						  handle_too_few_args(argv, i);
						  exec_cmd(argv);
						  break;
						}
					  
					  if (i < 3)
						{
						  handle_redirections(argv[optind], i, argv);
						}
					  else if (i == 3)
						{
						  cmd_list.cmds[cmd_list.cmd_count].argv_start_index = optind;
						}
					}
				}
				break;
			  }
			break;
		  default:
			fprintf(stderr, "?? getopt returned character code 0x%x\n", optc);
			exit(EXIT_FAILURE);
		  }

	  }
	while (true);

}
