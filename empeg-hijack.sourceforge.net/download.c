// empeg flash downloader (hugo@empeg.com)
//
// (C) 1999 empeg ltd
//
// unitdy but it works.

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char *serial_device = "/dev/ttyS0";
static char magic[] = "peace...";
static int  port;

#define FBLOCKSZ 16384	/* blocksize for programming flash memory */
static unsigned char outbuf[FBLOCKSZ+1];
static int outx = 0;

static void
openport (const char *dev)
{
	struct termios t;
	if ((port = open(dev, O_RDWR)) < 0) {
		perror(dev);
		exit(-1);
	}
	tcgetattr(port,&t);
	cfmakeraw(&t);
	cfsetispeed(&t,B115200);
	cfsetospeed(&t,B115200);
	tcsetattr(port,TCSANOW,&t);
}

static void
writeflush (void)
{
	int sent = 0, rc;

	while (sent < outx) {
		rc = write(port, &outbuf[sent], outx - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				continue;
			perror("writeflush failed");
			exit(-1);
		}
		sent += rc;
	}
	outx = 0;
}

static int readbyte (int msecs)
{
	struct timeval	timeout;
	fd_set		fds;
	char		d;

	writeflush();

	/* Make the set */
	FD_ZERO(&fds);
	FD_SET(port, &fds);

	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec  =  msecs / 1000;
	timeout.tv_usec = (msecs % 1000) * 1000;

	/* wait for input */
	switch (select(port+1, &fds, NULL, NULL, &timeout)) {
		case 1:
			if (1 != read(port, &d, 1))	/* read/return one byte */
				break;
			return d;
		case 0:
			return -1;	/* timed-out */
		default:
			break;
	}
	perror(serial_device);
	exit(-1);
}

static void
writebyte (unsigned char b)
{
	/* Write byte to port */
	outbuf[outx++] = b;
}

static void
readflush(void)
{
	while (readbyte(100) >= 0);
}

static int
readhex (int digits)
{
	char	buffer[16];
	int	a, d, result;

	// Read hex data
	for (a = 0; a < digits; a++) {
		if ((d = readbyte(3000)) <= 0)
		       return -1;
		buffer[a] = d;
	}
	buffer[digits] = 0;
	if (1 != sscanf(buffer, "%x", &result))
		result = -2;
	return result;
}

static void
readback (const char *s, int c)
{
	int rb;

	if ((rb = readbyte(3000)) != c) {
		fprintf(stderr, "\nReadback \"%s\" mismatch: expected '%c' got '%c'\n", s, c, rb);
		exit(-1);
	}
}

static void
send_command (unsigned char command, int address)
{
	// Send command
	readflush();
	writebyte(command);
	readback("command", command);

	// Send address
	writebyte(address      );
	writebyte(address >>  8);
	writebyte(address >> 16);
	writebyte(address >> 24);
	writeflush();

	// Get 8-byte hex address reply and see if it matches
	if (readhex(8) != address) {
		fprintf(stderr, "\nAddress echoback failed\n");
		exit(-1);
	}

	// Should now have a '-' showing it is working
	readback("dash1", '-');

	if (command != 'p') {
		int res = readhex(2);
		if (res != 0x80) {
			fprintf(stderr, "Bad result code: 0x%02x\n", res);
			exit(-1);
		}
	}
}

static void
program (char *buffer, int address, int length)
{
	int checksum,a;

	send_command('p', address);

	// Send length
	writebyte(length);
	writebyte(length >> 8);
	writeflush();

	// Get 4-char hex length reply and see if it matches
	if (readhex(4) != length) {
		fprintf(stderr, "Length echoback mismatch\n");
		exit(-1);
	}

	// Should now have a '-' showing it's waiting for data
	readback("dash2", '-');

	// Send data
	for(a = checksum = 0; a < length; a++) {
		checksum += buffer[a];
		writebyte(buffer[a]);
	}

	// Send checksum
	writebyte(checksum);

	// Should get an 'ok' indicating good download
	readback("o1", 'o');
	readback("k1", 'k');
	readback("\\r", '\r');
	readback("\\n", '\n');

	// Should have program reply within a second
	readback("o2", 'o');
	readback("k2", 'k');
}

static void
wait_for_prompt (const char *s, int prompt)
{
	int d;

	while((d = readbyte(3000)) > 0 && d != prompt);
	if (d < 0) {
		printf("Couldn't find %s ('%c')\n", s, prompt);
		exit(-1);
	}
}

int main(int argc, char *argv[])
{
	FILE	*infile;
	int	d, start, length, done, magicpos;
	int	manufacturer, product;
	int	erase_pos, erase_end, pagesize, lock_pos, lock_end;

	if (argc < 3 || argc > 4) {
		fprintf(stderr,"usage %s <file> <address in hex> [serial_device]\n", argv[0]);
		exit(1);
	}

	if (argc == 4)
		serial_device = argv[3];

	/* Open player device & setup */
	openport(serial_device);

	/* Tell user to power up the player, and wait for magic string */
	printf("Turn on empeg unit now\n");
	for (magicpos = 0; magic[magicpos]; ) {
		if ((d = readbyte(3000)) < 0)
			magicpos = 0;
		else if (d == magic[magicpos])
			magicpos++;
		else
			magicpos = (d == magic[0]);
	}

	printf("Found empeg unit: entering program mode\n");
	writebyte(1);
	writeflush();

	/* Manufacturer ID: Look for '#' then 4 hex digits */
	wait_for_prompt("manufacturer ID", '#');
	manufacturer = readhex(4);

	/* Product ID: Look for '#' then 4 hex digits */
	wait_for_prompt("product ID", '#');
	product = readhex(4);

	wait_for_prompt("prompt", '?');
	printf("Manufacturer=%04x, product=%04x\nStarting erase [", manufacturer, product);
	fflush(stdout);

	/* Size binary file */
	if ((infile = fopen(argv[1],"rb")) == NULL) {
		fprintf(stderr,"Can't open input '%s'\n", argv[1]);
		exit(3);
	}

	fseek(infile, 0L,SEEK_END);
	length = ftell(infile);
	fseek(infile, 0L, SEEK_SET);

	/* Get address */
	if (sscanf(argv[2], "%x", &start) != 1) {
		fprintf(stderr,"Can't parse start address '%s'\n",argv[2]);
		exit(4);
	}

	/* Erase pages that need erasing */
	lock_pos = erase_pos = (start & ~0x1fff);
	lock_end = erase_end = (start + length - 1) & ~0x1fff;

	for (done = 0; erase_pos <= erase_end;) {
		/* Work out the pagesize for this bit */
		pagesize = (erase_pos < 0x10000) ? 0x2000 : 0x10000;

		/* If we've got a C3 flash, we need to unlock the page first */
		if (product == 0x88c1)
			send_command('u', erase_pos);

		/* Do the erase */
		send_command('e', erase_pos);
		erase_pos += pagesize;
		done += pagesize;

		printf("%3d%%]\010\010\010\010\010", (done * 100) / length);
		fflush(stdout);
	}

	/* Erase done */
	printf("100%%] erase ok\nStarting program at 0x%x [", start);
	fflush(stdout);

	/* Do programming */
	for (done = 0; done < length;) {
		char block[FBLOCKSZ];
		int blocklength = ((length - done) > FBLOCKSZ) ? FBLOCKSZ : (length-done);

		if (blocklength&1)
			blocklength++;

		/* Read block */
		fread(block, 1, blocklength, infile);

		/* Program data */
		program(block, start, blocklength);

		start += blocklength;
		done  += blocklength;
		printf("%3d%%]\010\010\010\010\010", (done * 100) / length);
		fflush(stdout);
	}
	fclose(infile);

	/* C3 flash? */
	if (product == 0x88c1) {
		/* Lock all the pages again */
		while (lock_pos <= lock_end) {
			/* Work out the pagesize for this bit */
			pagesize = (lock_pos <0x10000) ? 0x2000 : 0x10000;
			send_command('l', lock_pos);
			lock_pos += pagesize;
		}
	}

	printf("100%%] program ok\n");

	sleep(1);
	writebyte('r');
	writebyte('\n');
	writeflush();
	sleep(1);
	close(port);
	return 0;
}
