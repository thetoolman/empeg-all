// mp3mobile: Drive VFD display
// altman@cryton.demon.co.uk
//
// Wiring: reset on bit 2, data on bit 1, clock on bit 0 of parallel port
// Compile with gcc -O2 display.c   (-O2 is required for port access)
// Must be run as root
//
#include <asm/io.h>
#include <unistd.h>
#include <stdio.h>

// LPT1: port address
static int port=0x378;

// Clock a byte out of the port MSB first
static void byte(int b)
  {
  int a,bit;

  for(a=7;a>=0;a--)
    {
    bit=(b&(1<<a));
    outb(0x4|(bit?2:0),port);
    outb(0x5|(bit?2:0),port);
    outb(0x4|(bit?2:0),port);
    }

  // inter-character delay: usleep() is too long for this, so this null
  // loop suffices for a P166. Your mileage may vary: if your screen is
  // blank, try increasing this.
  for(a=0;a<3000;a++);
  }

// Clock a string out, but only a maximum of 16 characters
static void string(char *s)
  {
  int maxlen=16;
  while(*s && maxlen>0) { byte(*s++); maxlen--; }
  }

void main(int argc,char *argv[])
  {
  // Takes one argument, a string to display
  if (argc!=2) return;
 
  // Grab parallel port
  if (ioperm(port,3,1)<0)
    {
    printf("ioperm error\n");
    exit(1);
    }

  // Reset the port
  outb(0x4,port);
  usleep(100);
  outb(0x0,port);
  usleep(100);
  outb(0x4,port);

  // Post reset delay
  usleep(1000);

  // Set brightness of display to maximum
  byte(255);

  // Show string
  string(argv[1]);
  exit(1);
  }

