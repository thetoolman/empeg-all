// mp3mobile: rxaudio interface
// altman@cryton.demon.co.uk

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include <asm/io.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>

#undef DEBUG_KEYPAD
#undef DEBUG_DISPLAY
#undef DEBUG_MESSAGES

#define VOLUME_NORMAL       100
#define VOLUME_MUTED        30

/* Base path of songs */
static char base_path[]="/music/";

/* States of input */
#define STATE_NORMAL           0
#define STATE_COMMAND          1

/* States of player */
typedef enum
  {
  PLAYER_NOTUNE,
  PLAYER_STOPPED,
  PLAYER_PLAYING,
  PLAYER_PAUSED,
  PLAYER_FINISHED,
  } player_state;

/* Current playing song structure */
typedef struct
  {
  char filename[256];
  char artist[64],song[64];	/* Song info, from ID3 */
  int year;

  int duration;			/* Length in seconds */
  int offset,range;		/* Current position in song, in frames */
  int h,m,s;			/* Current position in song, in hms */

  struct			/* Scrolling text message */
    {
    char text[128];
    int length,offset;
    } scroll;

  } songinfo;

/* Info on this song and on the database */
static songinfo current_song,song_database[1000];
static int song_noof=0,song_now=0;

/* Artist index */
typedef struct
  {
  char artist[64];		/* Artist name */
  int start;			/* Where their songs start */
  int noof;			/* Number of their songs */
  } artistinfo;

static artistinfo artist_database[1000];
static int artist_noof=0;

/* Playlist */
static int playlist[1000],playlist_position=0,playlist_length=0;

static char temp_message[32]="",command_message[64]="";
static struct timeval temp_timeout,scroll_timeout;
static int port=0x378;
static int keypad=0;

static player_state player;
static int state=STATE_NORMAL,substate[4];
static int player_volume=VOLUME_NORMAL;

// PID of player for killing it when we exit
static int player_pid;

// Currently selected artist/song for browsing
static int selected_artist=0,selected_song=0;

// Interface to rxaudio, uses ptys as pre-1.0 versions of rxaudio had buffering
// problems. Simplistic approach to pty/ttys, too :-)
static int pty,tty;

// Kill the player process (rxaudio)
static int player_kill()
  {
  kill(player_pid,SIGINT);
  }

// Get a message from the player: either doesn't block (nothing waiting) or blocks until
// a whole line is received
static int player_get(char *buffer,int maxlen)
  {
  struct timeval timeout;
  fd_set fds;

  /* Make the set */
  FD_ZERO(&fds);
  FD_SET(tty,&fds);

  /* Make the timeout */
  timeout.tv_sec=0;
  timeout.tv_usec=0;

  /* Do a select on the player input */
  if (select(tty+1,&fds,NULL,NULL,&timeout))
    {
    char d,*end=buffer+maxlen;

    /* Read a line */
    while(buffer<end && read(tty,&d,1)>0)
      {
      /* Newline? */
      if (d=='\n')
	{
	*buffer=0;
	return(1);
	}
      
      /* Otherwise store it (unless it's a CR) */
      if (d!='\r') *buffer++=d;
      }
    }

  /* Didn't get anything */
  return(0);
  }

// Deal with messages from player: calls player_display which can force an update
// of VFD display (eg: time elapsed). Updates current_song structure (a songinfo)
// with current info.
static void player_process(char *b)
  {
  if (strncmp(b,"MSG notify ",11)==0)
    {
    /* MSG notify */
    if (strncmp(b+11,"timecode",8)==0)
      {
      /* Note current offset */
      current_song.h=atoi(b+21);
      current_song.m=atoi(b+24);
      current_song.s=atoi(b+27);

      /* Update display */
      player_display(1);
      }
    else if (strncmp(b+11,"position",8)==0)
      {
      int offset,range;

      /* Note positon */
      if (sscanf(b+21,"offset=%d, range=%d",&offset,&range)==2)
	{
	current_song.offset=offset;
	current_song.range=range;
	}
      }
    else if (strncmp(b+11,"duration",8)==0)
      {
      /* Note duration */
      current_song.duration=atoi(b+21);
      }
    else if (strncmp(b+11,"player state",12)==0)
      {
      /* Note new state */
      if (strncmp(b+25,"EOF",3)==0)
	{
	player=PLAYER_FINISHED;
	}
      else if (strncmp(b+25,"PLAYING",7)==0)
	{
	player=PLAYER_PLAYING;

	/* Set volume */
	player_command("volume %d 100 50",player_volume);
	}
      else
	{
#ifdef DEBUG_MESSAGES
	printf("state: %s\n",b+18);
#endif
	}
      }
    else
      {
#ifdef DEBUG_MESSAGES
      printf("notify: %s\n",b+11);
#endif
      }
    }
  else
    {
#ifdef DEBUG_MESSAGES
    printf("Message from player: '%s'\n",b);
#endif
    }
  }

// Send a command to the player: you have to put a '\n' at the end.
static void player_put(char *command)
  {
  /* Send it */
  write(tty,command,strlen(command));
  }

// Issue a command to the player, and return 0/1 depending on result
// this also calls player_process() which process other messages received from the
// player before the ACK/NACK was received (eg: player time elapsed, etc).
static int player_command(char *format,...)
  {
  char buffer[512];
  va_list arg_pointer;

  va_start(arg_pointer,format);
  vsprintf(buffer,format,arg_pointer);
  va_end(arg_pointer);

  /* Send it, with a LF */
  player_put(buffer);
  player_put("\n");

  /* Wait for ack/nack */
  while(1)
    {
    /* Wait for a line */
    while(player_get(buffer,sizeof(buffer))==0) usleep(1000);

    /* ACK/NACK? */
    if (strncmp(buffer,"MSG notify ack",14)==0) return(0);
    else if (strncmp(buffer,"MSG notify nack",15)==0) return(1);
    player_process(buffer);
    }
  }

// Launch player, wait until it greets us
static void player_open()
  {
  char buffer[256];

  /* Open pty/tty pair to talk to player */
  if ((pty=open("/dev/ptyyf",O_RDWR))<0)
    {
    fprintf(stderr,"Can't open pty\n");
    exit(1);
    }
  if ((tty=open("/dev/ttyyf",O_RDWR))<0)
    {
    fprintf(stderr,"Can't open tty\n");
    exit(1);
    }

  /* Set PTY/TTY into raw mode */
  tcgetattr(pty,&t); cfmakeraw(&t); tcsetattr(pty,TCSANOW,&t);
  tcgetattr(tty,&t); cfmakeraw(&t); tcsetattr(tty,TCSANOW,&t);

  /* Fork off, so we can run the player task */
  if ((player_pid=fork())==0)
    {
    char *buffer[10];

    /* Use pty for io */
    dup2(pty,0);
    dup2(pty,1);

    /* Run the command */
    buffer[0]="rxaudio";
    buffer[1]=NULL;
    execv("/usr/local/bin/rxaudio",buffer);

    /* We never get here */
    while(1);
    }

  /* Kill player on exit */
  atexit(player_kill);

  /* Wait for RXaudio to come up */
  while(player_get(buffer,sizeof(buffer))==0) sleep(1);
  if (strcmp(buffer,"MSG notify ready")!=0)
    {
    fprintf(stderr,"rxaudio didn't respond correctly - exitting\n");
    exit(1);
    }

  // At this point, you can do things like:
  //
  // if (player_command("open %s",filename)==0)
  //   player_command("play");
  //
  // to ask the player to open a file and play it. Your main loop should be
  // be doing this at least:
  //
  // while(1)
  //   {
  //   if (player_get(buffer,sizeof(buffer)))
  //     {
  //     // process data from player, eg elapsed time
  //     player_process(buffer);
  //     }
  // 
  //   // Check keypad, update display, etc
  //   }
  //
  }

// Open a file, read .mp3, etc
static int player_open(char *tune)
  {
  char buffer[256];

  /* Read ID3 */
  if (id3_read(tune,&current_song)) return(1);

  /* Make scrolltext */
  sprintf(current_song.scroll.text,"%s / %s    ",
	  current_song.song,current_song.artist);
  current_song.scroll.length=strlen(current_song.scroll.text);
  current_song.scroll.offset=0;

  /* Ask player to open the file */
  sprintf(buffer,"open %s%s",base_path,tune);
  return(player_command(buffer));
  }

