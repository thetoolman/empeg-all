// aniwrite v1.0 (c) by Mark Lord, all rights reserved
// Empeg/RioCar utility for customizing the boot-up animation
// This REQUIRES and works only with a Hijack kernel installed on the player
// You have permission to copy and use this utility for personal use only.

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char help_msg[] = "\nUsage: aniwrite  rawfile\n   or: aniwrite  clear\n\n";
static const char open_err[] = "Error opening input file\n";
static const char read_err[] = "Error reading input file\n";
static const char kern_err[] = "Error opening /proc/empeg_kernel\n";
static const char seek_err[] = "Seek error on /proc/empeg_kernel\n";
static const char write_err[] = "Error writing /proc/empeg_kernel\n";
static unsigned char buf[0x20000];

int main (int argc, char *argv[])
{
	const char *afile = *++argv;
	unsigned int offset, *sigp;
	int clear = 0, bufsize, fd;

	if (argc < 2) {
		write(2, help_msg, sizeof(help_msg));
		exit(-1);
	}
	fd = open(afile, O_RDONLY);
	if (fd == -1) {
		if (afile[0] == 'c' && afile[1] == 'l' && afile[2] == 'e' && afile[3] == 'a' && afile[4] == 'r') {
			clear = 1;
		} else {
			write(2, open_err, sizeof(open_err));
			exit(-1);
		}
	}
	if (clear) {
		offset = 0xa0000 - 8;
		bufsize = 8;
		buf[0] = buf[1] = buf[2] = buf[3] = buf[4] = buf[5] = buf[6] = buf[7] = 0;
	} else {
		bufsize = read(fd, buf, sizeof(buf));
		if ((bufsize & 3) != 0 || bufsize < 1032 || bufsize >= (sizeof(buf) - 8)) {
			write(2, read_err, sizeof(read_err));
			exit(-1);
		}
		close(fd);

		offset = 0xa0000 - bufsize - 8;
		*((unsigned int *)&buf[bufsize]) = offset;
		bufsize += sizeof(unsigned int);
		buf[bufsize++] = 'A';
		buf[bufsize++] = 'N';
		buf[bufsize++] = 'I';
		buf[bufsize++] = 'M';
	}
	fd = open("/proc/empeg_kernel", O_RDWR|O_EXCL);
	if (fd == -1) {
		write(2, kern_err, sizeof(kern_err));
		exit(-1);
	}
	if ((off_t)-1 == lseek(fd, offset, SEEK_SET)) {
		write(2, seek_err, sizeof(seek_err));
		exit(-1);
	}
	if (bufsize != write(fd, buf, bufsize)) {
		write(2, write_err, sizeof(write_err));
		exit(-1);
	}
	fsync(fd);
	close(fd);
	sync();
	exit(0);
}

