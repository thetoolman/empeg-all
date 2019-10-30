// empeg flash downloader (hugo@empeg.com)
//
// (C) 1999 empeg ltd
//
// unitdy but it works.

#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef WIN32
#define DEVICE "\\\\.\\COM1"
HANDLE hPort;
OVERLAPPED overlapped;
#else
#define DEVICE "/dev/ttyS0"
int port;
#endif

char magic[]="peace...";

#ifdef WIN32

/* These badly need some error checking */

static int openport()
{
	DCB dcb;
	char buffer[32];
	hPort = CreateFile(DEVICE, GENERIC_READ | GENERIC_WRITE,
					  0 /* No sharing */, NULL /* Default security */,
					  OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
					  NULL /* no template */);
	if (hPort == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Failed to open COM port.\n");
		return -1;
	}

	if (!GetCommState(hPort, &dcb))
	{
		CloseHandle(hPort);
		fprintf(stderr, "Failed to get comm state.\n");
		return -1;
	}

	dcb.BaudRate = CBR_115200;
	dcb.fBinary = TRUE;
	dcb.fParity = FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fErrorChar = FALSE;
	dcb.fNull = FALSE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fAbortOnError = FALSE;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	if (!SetCommState(hPort, &dcb))
	{
		CloseHandle(hPort);
		fprintf(stderr, "Failed to set comm state.\n");
		return -1;
	}

	overlapped.hEvent = CreateEvent(NULL,  TRUE /* manual reset */,
									FALSE, /* initially non-signalled */
									NULL);  
	if (overlapped.hEvent == NULL)
	{
		CloseHandle(hPort);
		fprintf(stderr, "Failed to create event.\n");
		return -1;
	}
	return 0;
}

static void closeport()
{
	CloseHandle(overlapped.hEvent);
	overlapped.hEvent = NULL;
	CloseHandle(hPort);
	hPort = INVALID_HANDLE_VALUE;
}

static int readbyte(int wait)
{
	DWORD dwRead;
	DWORD dwBytes;
	unsigned char ch = 0;
	DWORD dwErrors;
	
	ClearCommError(hPort, &dwErrors, NULL);
	if (dwErrors)
		fprintf(stderr, "Error status in readbyte %ld\n", dwErrors);

	if (!ReadFile(hPort, &ch, 1, &dwRead, &overlapped))
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			// We've been deferred, wait for it.
			switch(WaitForSingleObject(overlapped.hEvent, wait * 1000))
			{
			case WAIT_OBJECT_0:
				/* We succeeded */
				break;
			case WAIT_TIMEOUT:
				/* We timed out */
				//fprintf(stderr, "Read timed out\n");
				PurgeComm(hPort, PURGE_RXABORT);
				//CancelIo(hPort);
				return -1;
			case WAIT_ABANDONED:
				/* We were abandoned, did the hEvent go away? */
				fprintf(stderr, "WaitForSingleObject abandoned\n");
				PurgeComm(hPort, PURGE_RXABORT);
				//CancelIo(hPort);
				return -1;
			case WAIT_FAILED:
			default:
				fprintf(stderr, "WaitForSingleObject failed %d\n", GetLastError());
				PurgeComm(hPort, PURGE_RXABORT);
				//CancelIo(hPort);
				return -1;
			}

			if (!GetOverlappedResult(hPort, &overlapped, &dwBytes, TRUE))
			{
				fprintf(stderr, "GetOverlappedResult failed with error %ld\n", GetLastError());
				return -1;
			}
			if (!dwBytes)
			{
				fprintf(stderr, "Read 0 bytes.\n");
				return -1;
			}
		}
		else
		{
			fprintf(stderr, "Read failed %ld\n", GetLastError());
			return -1;
		}
	}
	else
	{
		if (dwRead == 0)
		{
			fprintf(stderr, "Strange, we got a zero byte read.\n");
			return -1;
		}
	}
	return ch;
}

static void writebyte(int byte)
{
	DWORD dwWritten;
	unsigned char ch = byte;
	DWORD dwErrors;

	ClearCommError(hPort, &dwErrors, NULL);
	if (dwErrors)
		fprintf(stderr, "CommError in writebyte %ld\n", dwErrors);
	
	if (!WriteFile(hPort, &ch, 1, &dwWritten, &overlapped))
	{
		DWORD dwBytes;
		if (GetLastError() == ERROR_IO_PENDING)
		{
			// We've been deferred, wait for it.
			if (WaitForSingleObject(overlapped.hEvent, 10000) != WAIT_OBJECT_0)
			{
				// We failed. Cancel it.
				PurgeComm(hPort, PURGE_TXABORT);
				//CancelIo(hPort);
				fprintf(stderr, "WaitForSingleObject failed with error %d\n", GetLastError());
			}
			if (!GetOverlappedResult(hPort, &overlapped, &dwBytes, FALSE))
			{
				fprintf(stderr, "GetOverlappedResult failed with error %ld\n", GetLastError());
			}
			if (!dwBytes)
			{
				fprintf(stderr, "Wrote 0 bytes.\n");
			}
			else
			{
				//fprintf(stderr, "Write succeeded after time %c (%d).\n", ch, ch);
			}
		}
		else
		{
			fprintf(stderr, "Write failed %ld\n", GetLastError());
		}
	}
	else
	{
		if (dwWritten == 0)
			fprintf(stderr, "Strange, we got a zero byte write\n");
	}
}

#else /*WIN32*/

static int openport(void)
{
    struct termios t;
	if ((port=open(DEVICE,O_RDWR))<0)
		return -errno;

	tcgetattr(port,&t);
    cfmakeraw(&t);
    cfsetispeed(&t,B115200);
    cfsetospeed(&t,B115200);
    tcsetattr(port,TCSANOW,&t);

	return 0;
}

static void closeport()
{
	close(port);
}

static int readbyte(int wait)
  {
  struct timeval timeout;
  fd_set fds;

  /* Make the set */
  FD_ZERO(&fds);
  FD_SET(port,&fds);

  /* Make the timeout */
  timeout.tv_sec=wait;
  timeout.tv_usec=0;

  /* Do a select on the player input */
  if (select(port+1,&fds,NULL,NULL,&timeout))
    {
    /* Read byte */
    char d;
    read(port,&d,1);
    return(d);
    }

  /* Didn't get anything */
  return(-1);
  }

static void writebyte(int byte)
  {
  /* Write byte to port */
  char d=byte;
  write(port,&d,1);
  }

#endif /*WIN32*/

static void flushrx()
  {
  while(readbyte(0)>=0);
  }

static int readhex(int timeout,int digits)
  {
  char buffer[16];
  int a,d,result;

  // Read hex data
  for(a=0;a<digits;a++)
    {
    if ((d=readbyte(timeout))<0) return(-1);
    buffer[a]=d;
    }
  buffer[digits]=0;

  // Parse it
  if (sscanf(buffer,"%x",&result)!=1) return(-2);
  return(result);
  }

static int erasepage(int address)
  {
  int res;

  // Empty buffer first
  flushrx();

  // Send command
  writebyte('e');
  if (readbyte(1)!='e')
  {
	  fprintf(stderr, "Failed to get echo-back on erase command.\n");
	  return(1);
  }
  
  // Send address
  writebyte((address>> 0)&0xff);
  writebyte((address>> 8)&0xff);
  writebyte((address>>16)&0xff);
  writebyte((address>>24)&0xff);

  // Get 8-byte hex address reply and see if it matches
  if (readhex(1,8)!=address) return(2);

  // Should now have a '-' showing it's working on the erase
  if (readbyte(1)!='-') return(3);

  // Read result (leave 2 seconds)
  if ((res=readhex(2,2))!=0x80) return(0x100+res);

  return(0);
  }

static int lockpage(int address, int locktype)
{
  int res,command=(locktype==0?'u':'l');

  // Empty buffer first
  flushrx();

  // Send command
  writebyte(command);
  if (readbyte(1)!=command)
  {
	  fprintf(stderr, "Failed to get echo-back on lock/unlock command.\n");
	  return(1);
  }
  
  // Send address
  writebyte((address>> 0)&0xff);
  writebyte((address>> 8)&0xff);
  writebyte((address>>16)&0xff);
  writebyte((address>>24)&0xff);

  // Get 8-byte hex address reply and see if it matches
  if (readhex(1,8)!=address) return(2);

  // Should now have a '-' showing it's working on the lock
  if (readbyte(1)!='-') return(3);

  // Read result (leave 2 seconds)
  if ((res=readhex(2,2))!=0x80) return(0x100+res);

  return(0);
  }

static int program(char *buffer,int address,int length)
  {
  int checksum,a;

  // Empty buffer first
  flushrx();

  // Send command
  writebyte('p');
  if (readbyte(1)!='p') return(1);

  // Send address
  writebyte((address>> 0)&0xff);
  writebyte((address>> 8)&0xff);
  writebyte((address>>16)&0xff);
  writebyte((address>>24)&0xff);

  // Get 8-char hex address reply and see if it matches
  if (readhex(1,8)!=address) return(2);

  // Should now have a '-' showing it's waiting for length
  if (readbyte(1)!='-') return(3);

  // Send length
  writebyte((length>> 0)&0xff);
  writebyte((length>> 8)&0xff);
  
  // Get 4-char hex length reply and see if it matches
  if (readhex(1,4)!=length) return(4);

  // Should now have a '-' showing it's waiting for data
  if (readbyte(1)!='-') return(5);

  // Send data
  for(a=checksum=0;a<length;a++)
    {
    checksum+=buffer[a];
    writebyte(buffer[a]);
    }

  // Send checksum
  writebyte(checksum&0xff);

  // Should get an 'ok' indicating good download
  if (readbyte(1)!='o') return(6);
  if (readbyte(1)!='k') return(7);
  if (readbyte(1)!='\r') return(8);
  if (readbyte(1)!='\n') return(9);

  // Should have program reply within a second
  if (readbyte(1)!='o') return(10);
  if (readbyte(1)!='k') return(11);
  
  // All ok
  return(0);
  }

int main(int argc, char *argv[])
{
  FILE *i;
  int d,err,start,length,done,magicpos;
  int manufacturer,product;
  int erase_pos,erase_end,pagesize,lock_pos,lock_end;

  if (argc<3 || argc>4) {
    fprintf(stderr,"usage %s <file> <address in hex> [direct = we're already in the flasher]\n", argv[0]);
    exit(1);
  }
  
  /* Open player device & setup */
  if (openport()) {
    fprintf(stderr,"Couldn't open device port (%s)\n",DEVICE);
    exit(2);
  }
  
  if (argc<4) {
    /* Tell user to power up the device */
    printf("Turn on empeg unit now\n");
    magicpos=0;
    while(1) {
      /* Look for magic string */
      d=readbyte(1);
      if (d<0) magicpos=0;
      else {
	if (d==magic[magicpos]) {
	  magicpos++;
	  if (magic[magicpos]==0) break;
	}
	else magicpos=(d==magic[0])?1:0;
      }
    }
    
    printf("found empeg unit: entering program mode\n");
    writebyte(1);

    /* Manufacturer ID: Look for '#' then 4 hex digits */
    while((d=readbyte(1))>0 && d!='#');
    if (d<0) {
      printf("Couldn't find manufacturer ID!\n");
      exit(10);
    }
    manufacturer=readhex(1,4);

    /* Product ID: Look for '#' then 4 hex digits */
    while((d=readbyte(1))>0 && d!='#');
    if (d<0) {
      printf("Couldn't find product ID!\n");
      exit(11);
    }
    product=readhex(1,4);
    
    printf("manufacturer=%04x, product=%04x\n",manufacturer,product);
  } else {
    /* Send a cr to get a prompt back */
    writebyte(13);
  }

  printf("waiting for prompt\n");
  while((d=readbyte(1))>0 && d!='?');
  if (d<0) {
    printf("No prompt within timeout\n");
    exit(12);
  }
  printf("starting erase [");
  fflush(stdout);
  
  /* Size binary file */
  if ((i=fopen(argv[1],"rb"))==NULL) {
    fprintf(stderr,"Can't open input '%s'\n",argv[1]);
    exit(3);
  }
  
  fseek(i,0L,SEEK_END);
  length=ftell(i);
  fseek(i,0L,SEEK_SET);
  
  /* Get address */
  if (sscanf(argv[2],"%x",&start)!=1) {
    fprintf(stderr,"Can't parse start address '%s'\n",argv[2]);
    exit(4);
  }
  
  /* Erase pages that need erasing */
  lock_pos=erase_pos=(start&~0x1fff);
  lock_end=erase_end=(start+length-1)&~0x1fff;

  done=0;
  while(erase_pos<=erase_end) {
      /* Work out the pagesize for this bit */
      pagesize=(erase_pos<0x10000)?0x2000:0x10000;

      /* If we've got a C3 flash, we need to unlock the page first */
      if (product==0x88c1) {
	if ((err=lockpage(erase_pos,0))!=0) {
	  fprintf(stderr,"lockpage(%x,0) got code %x\n",erase_pos,err);
	  exit(5);
	  }
      }
	
      /* Do the erase */
      if ((err=erasepage(erase_pos))!=0) {
	fprintf(stderr,"erasepage(%x) got code %x\n",erase_pos,err);
	exit(5);
      }

      erase_pos+=pagesize;
      done+=pagesize;

      printf("%3d%%\010\010\010\010",(done*100)/length);
      fflush(stdout);
  }
	   
  /* Erase done */
  printf("100%%] erase ok\nstarting program at 0x%x [",start);
  fflush(stdout);
  
  /* Do programming */
  done=0;
  while(done<length) {
    char block[16384];
    int blocklength=((length-done)>16384)?16384:(length-done);
    
    if (blocklength&1) blocklength++;

    /* Read block */
    fread(block,1,blocklength,i);

    /* Program data */
    if ((err=program(block,start,blocklength))!=0) {
      fprintf(stderr,"program(%x) got code %x\n",start,err);
      exit(6);
    }
    
    start+=blocklength;
    done+=blocklength;
    printf("%3d%%\010\010\010\010",(done*100)/length);
    fflush(stdout);
  }
  fclose(i);
  
  /* C3 flash? */
  if (product==0x88c1) {
    /* Lock all the pages again */
    while(lock_pos<=lock_end) {
      /* Work out the pagesize for this bit */
      pagesize=(lock_pos<0x10000)?0x2000:0x10000;

      if ((err=lockpage(lock_pos,1))!=0) {
	fprintf(stderr,"lockpage(%x,1) got code %x\n",lock_pos,err);
	exit(5);
      }
      lock_pos+=pagesize;
    }
  }
    
  printf("100%%] program ok\n");
  
  closeport();
  return 0;
}



