// mp3mobile: keypad input
// altman@cryton.demon.co.uk

// File handle of keypad device
static int keypad=0;

// Open keypad and initialise to 1200 baud, raw mode.
static int keypad_initialise()
  {
  struct termios t;

  /* Open keypad port */
  if ((keypad=open("/dev/ttyS0",O_RDONLY))<0) return(1);

  /* Set raw/speed */
  tcgetattr(keypad,&t);
  cfmakeraw(&t);
  cfsetispeed(&t,B1200);
  cfsetospeed(&t,B1200);
  tcsetattr(keypad,TCSANOW,&t);

  return(0);
  }

// Get a message from the user: in fact, we only get sent single letter commands,
// but this is more general purpose. Checks for '!' at this low level and does
// an exit if so (user has turned ignition off). This doesn't block: blocking
// etc is done by things at higher levels controlling the speed of scrolling,
// and so on. It *will* block until it gets a whole line, however, for simplicity.
static int user_get(char *buffer,int maxlen)
  {
  struct timeval timeout;
  fd_set fds;

  /* Make the set */
  FD_ZERO(&fds);
  FD_SET(keypad,&fds);

  /* Make the timeout */
  timeout.tv_sec=0;
  timeout.tv_usec=0;

  /* Do a select on the player input */
  if (select(keypad+1,&fds,NULL,NULL,&timeout))
    {
    char d,*end=buffer+maxlen,*b=buffer;

    /* Read a line */
    while(b<end && read(keypad,&d,1)>0)
      {
      /* Newline? */
      if (d=='\r' || d=='\n')
	{
	*b=0;

	/* Check for power off */
	if (buffer[0]=='!')
	  {
	  /* Power off! Exit now! */
	  display_string("   Power Down");
	  exit(0);
	  }

	return(1);
	}
      
      /* Otherwise store it */
      *b++=d;
      }
    }

  /* Didn't get anything */
  return(0);
  }
