//
// mp3tool.c (c)2000-2004 Mark Lord <mlord@pobox.com>
// All Rights Reserved.
//
// This source code is non-redistributable!
// If you have a copy of this source file,
// you are NOT permitted to distribute it further.
//

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
   #define inline
   #define USE_STDIO
   #include <winsock2.h>
#else
#define USE_STDIO
   #include <unistd.h>
   #include <netinet/in.h>
#endif

#ifdef USE_STDIO
   #define FD			FILE *
   #define FD_FAILED		NULL
   #define close(f)		fclose(f)
   #define open(path,flags)	fopen(path,"rb")
   #define creat(path,flags)	fopen(path,"wb")
   #define read(f,buf,size)	fread(buf,1,size,f)
   #define write(f,buf,size)	fwrite(buf,1,size,f)
   #define mkstemp(path)	fopen(path,"wb")
   #define getstat(path,fd,ptr)	stat(path,ptr)
#else
   #define FD			int
   #define FD_FAILED		-1
   #define getstat(path,fd,ptr)	fstat(fd,ptr)
   #include <fcntl.h>
#endif

#define HDR_SIZE		10
#define TAG_UNSYNC		0x80
#define TAG_EXT_HDR		0x40
#define TAG_EXPERIMENTAL	0x20
#define TAG_FOOTER		0x10

#define EXT_HDR_SIZE		6
#define EXT_UPDATE		0x40
#define EXT_CRC_DATA		0x20
#define EXT_RESTRICTIONS	0x10

#define FRAME_HDR_SIZE		10
#define FRAME_ALTER		0x4000
#define FRAME_FILE_ALTER	0x2000
#define FRAME_READ_ONLY		0x1000
#define FRAME_GROUP_INFO	0x0040
#define FRAME_COMPRESSED	0x0008
#define FRAME_ENCRYPTED		0x0004
#define FRAME_UNSYNC		0x0002
#define FRAME_DATALEN		0x0001

static int config_headers   = 0;
static int config_mp3hdrs   = 0;
static int config_extract   = 0;
static int config_display   = 0;
static int config_rewrite   = 0;
static int config_repair    = 0;
static int config_remove    = 0;
static int config_datafiles = 0;

static void *Malloc (unsigned int bytecount)
{
	void *p = calloc(1,bytecount);
	if (p == NULL) {
		fprintf(stderr, "calloc() failed: no memory\n");
		exit(ENOMEM);
	}
	return p;
}

static FD Creat (char *path)
{
	FD fd = creat(path, O_RDONLY|O_EXCL);
	if (fd == FD_FAILED) {
		int err = errno;
		fprintf(stderr, "creat(\"%s\") failed: %s\n", path, strerror(err));
		exit(err);
	}
	fprintf(stderr, "Created: \"%s\"\n", path);
	return fd;
}

static FD Mkstemp (char *path)
{
	FD fd = mkstemp(path);
	if (fd == FD_FAILED) {
		int err = errno;
		fprintf(stderr, "Mkstemp(\"%s\") failed: %s\n", path, strerror(err));
		exit(err);
	}
	return fd;
}

static unsigned int Read (FD fd, void *buf, unsigned int bytecount, const char *msg)
{
	unsigned int rbytes, bytesleft = bytecount;

	while (bytesleft > 0 && (rbytes = read(fd, buf, bytesleft)) != 0) {
		if (rbytes > 0) {
			bytesleft -= rbytes;
		} else if (rbytes == -1 && errno != EAGAIN) {
			int err = errno;
			fprintf(stderr, "read(%s) failed: %s\n", msg, strerror(err));
			exit(err);
	      	}
	}
	return bytecount - bytesleft;
}

static void Write (FD fd, void *buf, unsigned int bytecount, const char *msg)
{
	while (bytecount > 0) {
		int wbytes = write(fd, buf, bytecount);
		if (wbytes > 0) {
			bytecount -= wbytes;
		} else if (wbytes == -1 && errno != EAGAIN) {
			int err = errno;
			fprintf(stderr, "write(%s) failed: %s\n", msg, strerror(err));
			exit(err);
	      	}
	}
}

static const char *get_pictype (char pictype)
{
	switch (pictype) {
		case 0x01: return "32x32 icon (PNG only) ";
		case 0x02: return "Other file icon ";
		case 0x03: return "Front Cover ";
		case 0x04: return "Back Cover ";
		case 0x05: return "Leaflet page ";
		case 0x06: return "Media Info ";
		case 0x07: return "Lead artist/performer ";
		case 0x08: return "Artist/performer ";
		case 0x09: return "Conductor ";
		case 0x0A: return "Band/Orchestra ";
		case 0x0B: return "Composer ";
		case 0x0C: return "Lyricist/text writer ";
		case 0x0D: return "Recording Location ";
		case 0x0E: return "During recording ";
		case 0x0F: return "During performance ";
		case 0x10: return "Movie/video screen capture ";
		case 0x11: return "A bright coloured fish ";
		case 0x12: return "Illustration ";
		case 0x13: return "Band/artist logotype ";
		case 0x14: return "Publisher/Studio logotype ";
		default:   return "";
	}
}

static int datafile_count = 0;
static char *save_datafile (const char *root_path, char *mimetype, void *data, int nbytes)
{
	FD	fd;
	char	*tname = NULL, *prefix = "", *suffix = "", *XXXXXX = "XXXXXX", *fname;

	if (mimetype != NULL) {
		if (!strcmp(mimetype, "image/jpeg"))
			mimetype = "image/jpg";
		prefix = strcpy(Malloc(strlen(mimetype)+1), mimetype);
		suffix = strchr(prefix, '/');
		if (suffix != NULL) {
			*suffix++ = '\0';
			if (*suffix == '.')
				++suffix;
			if (!*suffix || strchr(suffix, '/') != NULL)
				suffix = NULL;
		}
	}
	if (suffix == NULL)
		suffix = "";
	tname = Malloc(strlen(root_path)+1+strlen(XXXXXX)+1);
	sprintf(tname, "%s.%s", root_path, XXXXXX);
	fd = Mkstemp(tname);
	Write(fd, data, nbytes, "datafile");
	close(fd);
	fname = Malloc(strlen(root_path)+1+strlen(prefix)+3+strlen(suffix)+1);
	sprintf(fname, "%s.%s%02d", root_path, prefix, ++datafile_count % 100);
	if (*suffix) {
		strcat(fname, ".");
		strcat(fname, suffix);
	}
	if (mimetype != NULL)
		free(prefix);
	if (rename(tname, fname) != -1) {
		free(tname);
	} else {
		fprintf(stderr, "rename(%s,%s) failed: %s\n", tname, fname, strerror(errno)); 
		free(fname);
		fname = tname;
	}
	fprintf(stderr, "Created: %s\n", fname);
	return fname;
}

#define display_printf	if (config_display) printf
#define headers_printf	if (config_headers) printf

static struct v1tag_s {
	unsigned char header [ 3];
	unsigned char song   [30];
	unsigned char artist [30];
	unsigned char album  [30];
	unsigned char year   [ 4];
	unsigned char comment[28];
	unsigned char tracknr[ 2];
	unsigned char genre  [ 1];
} v1tag;

static void handle_frame (const char *root_path, unsigned char *ftype, int frame_size, unsigned char *buf)
{
	unsigned char encoding = buf[0], *text = buf + 1;

	if (ftype[0] == 'T' || ftype[0] == 'W' || !strcmp(ftype, "APIC")) {
		if (ftype[0] == 'W' && strcmp(ftype, "WXXX")) {
			encoding = 0;
			text = buf;
		}
		if (!strcmp(&ftype[1], "XXX") && encoding == 0 && *text) {
			/* ftype = "TXXX" or "WXXX" */
			unsigned char *nl;
			ftype = text;
			text = &text[1+strlen(ftype)];
			display_printf("%24s: (below)\n", ftype);
			do {
				nl = strchr(text, '\n');
				if (nl != NULL)
					*nl = '\0';
				display_printf("%24s  %s\n", " ", text);
				text = nl + 1;
			} while (nl != NULL);
			return;
		}
		if (!strcmp(ftype, "APIC")) { ftype = "Image-Attached";
			if (encoding == 0) {
				char *mimetype = text, *desc = text + strlen(mimetype) + 2;
				const char *pictype = get_pictype(text[strlen(mimetype) + 1]);
				int picsize = frame_size - (strlen(mimetype) + strlen(desc) + 3);
				text = desc+ strlen(desc) + 1;
				display_printf("%24s: %s%s (%u bytes)", ftype, mimetype, pictype, picsize);
				if (config_datafiles && picsize > 0) {
					char *f = save_datafile(root_path, mimetype, text, picsize);
					display_printf(", saved as '%s'", f);
					free(f);
				}
				display_printf("\n");
				if (*desc && strcmp(desc," ")) {
					display_printf("\n%24s: %s\n", "Image-Description", desc);
				}
				return;
			}
		} else if (!strcmp(ftype, "TALB")) { ftype = "Album";
			if (encoding == 0)
				strncpy(v1tag.album, text, sizeof(v1tag.album));
		} else if (!strcmp(ftype, "TCON")) { ftype = "Content-Type";
			if (encoding == 0 && text[0] == '(' && (text[2] == ')' || text[3] == ')'))
				v1tag.genre[0] = atoi(&text[1]);
		} else if (!strcmp(ftype, "TFLT")) { ftype = "File-Type";
		} else if (!strcmp(ftype, "TENC")) { ftype = "Encoded-By";
		} else if (!strcmp(ftype, "TIT1")) { ftype = "Category";
		} else if (!strcmp(ftype, "TIT2")) { ftype = "Title";
			if (encoding == 0)
				strncpy(v1tag.song, text, sizeof(v1tag.song));
		} else if (!strcmp(ftype, "TIT3")) { ftype = "Subtitle";
		} else if (!strcmp(ftype, "TKEY")) { ftype = "Initial-Key";
		} else if (!strcmp(ftype, "TLAN")) { ftype = "Language(s)";
		} else if (!strcmp(ftype, "TLEN")) { ftype = "Length";
			if (encoding == 0) {
				unsigned int ms = atoi(text), sec = ms / 1000;
				display_printf("%24s: %02d:%02d.%03d\n", ftype, sec/60, sec%60, ms%1000);
				return;
			}
		} else if (!strcmp(ftype, "TMED")) { ftype = "Original-Media-Type";
		} else if (!strcmp(ftype, "TOAL")) { ftype = "Original-Album-Title";
		} else if (!strcmp(ftype, "TOFN")) { ftype = "Original-File-Name";
		} else if (!strcmp(ftype, "TOLY")) { ftype = "Original-Lyricist";
		} else if (!strcmp(ftype, "TOPE")) { ftype = "Original-Performer";
		} else if (!strcmp(ftype, "TORY")) { ftype = "Original-Release";
		} else if (!strcmp(ftype, "TOWN")) { ftype = "Owner-Licensee";
		} else if (!strcmp(ftype, "TPE1")) { ftype = "Artist";
			if (encoding == 0)
				strncpy(v1tag.artist, text, sizeof(v1tag.artist));
		} else if (!strcmp(ftype, "TPE2")) { ftype = "Accompanied-By";
		} else if (!strcmp(ftype, "TPE3")) { ftype = "Other-Performer";
		} else if (!strcmp(ftype, "TPE4")) { ftype = "Modified-By";
		} else if (!strcmp(ftype, "TPOS")) { ftype = "Part-Of-A-Set";
		} else if (!strcmp(ftype, "TPUB")) { ftype = "Publisher";
		} else if (!strcmp(ftype, "TRCK")) { ftype = "Track-Number";
			if (encoding == 0)
				v1tag.tracknr[1] = atoi(text);
		} else if (!strcmp(ftype, "TRDA")) { ftype = "Recording-Dates";
		} else if (!strcmp(ftype, "TRSN")) { ftype = "Radio-Station";
		} else if (!strcmp(ftype, "TRSO")) { ftype = "Radio-Owner";
		} else if (!strcmp(ftype, "TSIZ")) { ftype = "Size";
		} else if (!strcmp(ftype, "TSRC")) { ftype = "ISRC-Code";
		} else if (!strcmp(ftype, "TSSE")) { ftype = "Encoder-Settings";
		} else if (!strcmp(ftype, "TYER")) { ftype = "Year";
			if (encoding == 0)
				strncpy(v1tag.year, text, sizeof(v1tag.year));
		} else if (!strcmp(ftype, "WCOM")) { ftype = "Commercial-Site";
		} else if (!strcmp(ftype, "WCOP")) { ftype = "Legal-Info-Site";
		} else if (!strcmp(ftype, "WOAF")) { ftype = "MP3-Files-Site";
		} else if (!strcmp(ftype, "WOAR")) { ftype = "Artist-Site";
			if (encoding == 0)
				strncpy(v1tag.comment, text, sizeof(v1tag.comment));
		} else if (!strcmp(ftype, "WOAS")) { ftype = "Source-Site:";
		} else if (!strcmp(ftype, "WORS")) { ftype = "Radio-Site:";
		} else if (!strcmp(ftype, "WPAY")) { ftype = "Payment-Site:";
		} else if (!strcmp(ftype, "WPUB")) { ftype = "Publisher-Site:";
		}
		if (encoding == 0) {
			display_printf("%24s: %s%s", ftype, text, (text[strlen(text)-1] == '\n') ? "" : "\n");
		} else {
			display_printf("%24s: [unsupported encoding (%d)]\n", ftype, encoding);
		}
		return;
	}
	       if (!strcmp(ftype, "COMM")) { ftype = "Comment";
	} else if (!strcmp(ftype, "USLT")) { ftype = "Lyrics";
	} else if (!strcmp(ftype, "USER")) { ftype = "Terms-Of-Use";
	} else if (!strcmp(ftype, "UFID")) {
		int i, start = strlen(buf) + 2, end = start + 64, use_hex = 0;
		ftype = "File-Id";
		if (end > frame_size)
			end = frame_size;
		for (i = start; i < end; ++i) {
			if (!isalnum(buf[i]))
				use_hex = 1;
		}
		display_printf("%24s: %s\n%24s: %s", "File-Owner", buf, ftype, use_hex ? "(hex)" : "");
		for (i = start; i < end; ++i) {
			display_printf(use_hex ? "%02x" : "%c", buf[i]);
		}
		display_printf("\n");
		return;
	} else if (!strcmp(ftype, "GEOB")) {
		char	*mimetype, *fname, *desc, *object;
		int	objsize;

		ftype = "General-Object";
		mimetype = text;
		fname = mimetype + strlen(mimetype) + 1;
		desc = fname + strlen(fname) + 1;
		object = desc + strlen(desc) + 1;
		objsize = frame_size - (strlen(mimetype) + strlen(fname) + strlen(desc) + 4);

		display_printf("%24s: %s (%u bytes)", ftype, mimetype, objsize);
		if (config_datafiles) {
			char *f = save_datafile(root_path, mimetype, object, objsize);
			display_printf(", saved as '%s'", f);
			free(f);
		}
		display_printf("\n");
		if (*fname && strcmp(fname," ")) {
			if (encoding == 0) {
				display_printf("%24s: '%s'\n", "Object-File-Name", fname);
			} else {
				display_printf("%24s: [unsupported encoding (%d)]\n", "Object-File-Name", encoding);
			}
		}
		if (*desc && strcmp(desc," ")) {
			if (encoding == 0) {
				display_printf("%24s: %s\n", "Object-Description", desc);
			} else {
				display_printf("%24s: [unsupported encoding (%d)]\n", "Object-Description", encoding);
			}
		}
		return;
	} else {
		display_printf("%24s: (length %d bytes)\n", ftype, frame_size);
		return;
	}
	/* ftype = "COMM" or "USLT" or "USER" */
	if (encoding != 0) {
		display_printf("%24s: [unsupported encoding (%d)]\n", ftype, encoding);
	} else {
		unsigned char *nl;
		display_printf("%24s: (%c%c%c) %s", ftype, text[0], text[1], text[2], text+3);
		text = text + strlen(text) + 1;
		if (config_datafiles && !strcmp(ftype, "Lyrics")) {
			char *f = save_datafile(root_path, "lyrics/text", text, strlen(text)+1);
			display_printf(", saved as '%s'\n", f);
			free(f);
		} else {
			char *endf = &buf[frame_size];
			display_printf("\n");
			while ((char *)text < endf && *text) {
				nl = strchr(text, '\n');
				if (nl != NULL)
					*nl = '\0';
				display_printf("%24s  %s\n", " ", text);
				text += strlen(text) + 1;
				if (nl == NULL)
					break;
			}
		}
	}
}

static unsigned char *get4bytes (unsigned char *src, unsigned int *dest)
{
	*dest = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	return (src + 4);
}

static unsigned char *put4bytes (unsigned int src, unsigned char *dest)
{
	*dest++ = (src >> 24) & 0xff;
	*dest++ = (src >> 16) & 0xff;
	*dest++ = (src >>  8) & 0xff;
	*dest++ =  src        & 0xff;
	return (dest);
}

static inline unsigned int syncsafe (void *from)
{
	unsigned char *c = (unsigned char *)from;
	unsigned int s = *c;
	s = (s << 7) + *++c;
	s = (s << 7) + *++c;
	s = (s << 7) + *++c;
	return s;
}

/*
 * Xing VBR Headers explained:
 *
 * Each frame represents exactly .026 seconds of playback time.
 * But frames in a VBR mp3 vary in size, according to the bit rate
 * and encoding methods used, on a frame by frame basis.
 *
 * This makes random seeks to percentage points within a VBR file
 * rather difficult without reading/decoding the file to count
 * frames up to the seek point -- which also requires knowing the
 * file size and frame count in advance. 
 *
 * The Xing VBR header eases this problem, using a header which
 * (optionally) provides a framecount (Xframes), byte count (Xbytes),
 * and a 100-position TOC for seeking to percentages with the file.
 * The seeks are not exactly dead on, so the software using them
 * must then scan for the next Sync pattern to find the next frame
 * after seeking to the byte offset indicated.
 *
 * The seek table itself does not store byte offsets, but rather
 * scale factors from 0 to 255 representing the seek points as the
 * number of 1/256's of the filesize.
 *
 * For example, to do a seek to the P% point, one would do
 * something like this to calculate the corresponding byte offset:
 *
 *	if (P <  0) P =  0;
 *	if (P > 99) P = 99;
 *	byteoffset = Xbytes * Xtoc[P] / 256;
 *
 * If the percentage is a fractional (floating point) value,
 * then we must interpolate between two table entries for
 * more accurate positioning:
 *
 *	int Pi; float Tp, Tr;
 *	if (P <   0.0) P =   0.0;
 *	if (P > 100.0) P = 100.0;
 *	Pi = (int)P;
 *	if (Pi >= 99) {
 *		Tp = Xtoc[Pi = 99];
 *		Tr = 256.0;
 *	} else {
 *		Tp = Xtoc[Pi];
 *		Tr = Xtoc[Pi+1];
 *	}
 *	P = ( (P - Pi) * (Tr - Tp) + Tp ) * (1.0 / 256.0);
 *	byteoffset = (int)(P * Xbytes);
 *
 * The Xing header also includes an "Xscale" value, the use for which
 * appears to be a complete mystery.  If anyone can explain to me what
 * this field is, and how to use it, then I will add code here to validate
 * and repair it as well.  For now, we mostly just leave it alone.
 * UPDATE: this indicates a "quality" factor from 0 (best) to 100 (worst).
 * Lame apparently appends a ton of other stuff in the the "Xing" (or "Info")
 * frame, for prescaling etc.. but we don't currently interpret any of it here.
 *
 * Note: older versions of this program insisted on having the Xing
 * header include itself in its framecount and bytecount.
 */

#define verbose_printf  if (verbose) printf

static int dump_Xing (const char *msg, unsigned char *Xing, unsigned int frames, unsigned int bytes, int verbose)
{
	int Xing_is_bad = 0;
	unsigned int Xflags, Xframes, Xbytes, Xscale;
	unsigned char *Xtoc = NULL;

	Xing += 4;
	Xing = get4bytes(Xing, &Xflags);
	verbose_printf("    %s:", msg);
	if ((Xflags & 1)) {
		Xing = get4bytes(Xing, &Xframes);
		verbose_printf(" Frames=%d", Xframes);
		if (Xframes != (frames - 1)) {
			verbose_printf("?");
			Xing_is_bad = 1;
		}
	}
	if ((Xflags & 2)) {
		Xing = get4bytes(Xing, &Xbytes);
		verbose_printf(" Bytes=%d", Xbytes);
		if (Xbytes != bytes) {
			verbose_printf("?");
			Xing_is_bad = 2;
		}
	}
	if ((Xflags & 4)) {
		Xtoc = Xing;
		Xing += 100;
	}
	if ((Xflags & 8)) {
		Xing = get4bytes(Xing, &Xscale);
		verbose_printf(" VBRscale=%d", Xscale);
	}
	if (Xtoc) {
		int	i, prev = -1;
		for (i = 0; i < 100; ++i) {
			if ((i % 10) == 0) {
				verbose_printf("\n        %4s %2d:",
					i ? "    " : "TOC:", i);
			}
			if (Xtoc[i] >= prev && (i == 0 || Xtoc[i])) {
				verbose_printf("%4d ", Xtoc[i]);
			} else {
				verbose_printf("%4d?", Xtoc[i]);
				Xing_is_bad = 4;
			}
			prev = Xtoc[i];
		}
	}
	verbose_printf("\n");
	return (Xing_is_bad);
}

static const char *versions[4] = {"2.5", "reserved", "2.0", "1.0"};
static const char *layers[4] = {"reserved", "III", "II", "I"};
static const char layervals[4] = {-1, 3, 2, 1};
static const char *protections[2] = {"CRC16", "noCRC"};
static const int bitrates[16][5] = {
	{0,	0,	0,	0,	0},
	{32,	32,	32,	32,	8},
	{64,	48,	40,	48,	16},
	{96,	56,	48,	56,	24},
	{128,	64,	56,	64,	32},
	{160,	80,	64,	80,	40},
	{192,	96,	80,	96,	48},
	{224,	112,	96,	112,	56},
	{256,	128,	112,	128,	64},
	{288,	160,	128,	144,	80},
	{320,	192,	160,	160,	96},
	{352,	224,	192,	176,	112},
	{384,	256,	224,	192,	128},
	{416,	320,	256,	224,	144},
	{448,	384,	320,	256,	160},
	{-1,	-1,	-1,	-1,	-1} };

static const int samplerates[4][3] = {
	{44100,	22050,	11025},
	{48000,	24000,	12000},
	{32000,	16000,	 8000},
	{-1,	-1,	-1} };

static const char *channelmodes[4] = {"Stereo", "JointStereo", "DualChannel", "Mono"};

static const char *jsextensions[2][4] = {
	{" bands 4-31",	" bands 8-31",	" bands 12-31",	" bands 16-31"},
	{"",	" Intensity",	" Middle/Side",	" Intensity+Middle/Side"} };

static const char *emphasiss[4] = {"none", "50/15ms", "reserved", "CCIT-J.17"};

static void process_mp3_headers (unsigned char *ibuf, unsigned int isize, unsigned int start_offset, int has_id3v1)
{
	const	unsigned int syncword = 0x7ff;
	int	brx;
	unsigned int hdrtype = 0;

	unsigned int hdr;
	unsigned int fsync;		/* 11: 31-21 */
	unsigned int version;		/*  2: 20-19 */
	unsigned int layer;		/*  2: 18-17 */
	unsigned int layerval;
	unsigned int protection;	/*  1: 16    */
	unsigned int bitrate;		/*  4: 15-12 */
	int bitrate_val;
	unsigned int samplerate;	/*  2: 11-10 */
	unsigned int padding;		/*  1:  9    */
	unsigned int private;		/*  1:  8    */
	unsigned int channelmode;	/*  2:  7-6  */
	unsigned int jsextension;	/*  2:  5-4  */
	unsigned int copyright;		/*  1:  3    */
	unsigned int original;		/*  1:  2    */
	unsigned int emphasis;		/*  2:  1-0  */
	unsigned int framecount = 0;
	unsigned char *start = ibuf;
	unsigned char *Xing_header = NULL;
	int kbps = -1;
	unsigned int kbps_total = 0, music_bytes = 0;

	unsigned int foffset = 0, *foffsets = NULL, foffsets_count = 0;

	int flen = 0, Xflags = 0, Xframes = -1, Xbytes = -1, Xscale = -1, Xing_is_bad = 0;
	unsigned char *Xtoc = NULL;
	unsigned int file_is_vbr = 0, end_check = 4, lost_sync = 0;

	if (has_id3v1)
		end_check += 128;
#if 0
	printf("Total MP3 stream bytes = %d at offset %d\n", isize, start_offset);
#endif

	while (isize >= end_check) {
		foffset = start_offset + ibuf - start;
		(void)get4bytes(ibuf, &hdr);
		++ibuf;
		--isize;
		fsync		= (hdr >> 21) & syncword;
		version		= (hdr >> 19) & 0x3;
		layer		= (hdr >> 17) & 0x3;
		protection	= (hdr >> 16) & 0x1;
		bitrate		= (hdr >> 12) & 0xf;
		samplerate	= (hdr >> 10) & 0x3;
		padding		= (hdr >>  9) & 0x1;
		private		= (hdr >>  8) & 0x1;
		channelmode	= (hdr >>  6) & 0x3;
		jsextension	= (hdr >>  4) & 0x3;
		copyright	= (hdr >>  3) & 0x1;
		original	= (hdr >>  2) & 0x1;
		emphasis	= (hdr >>  0) & 0x3;
		if (fsync == syncword && lost_sync)
			printf("    %08x: resync'd (%d bytes remaining)\n", hdr, isize);
		if (fsync != syncword) {
			if (lost_sync) continue; lost_sync = 1;
			printf("    %08x: lost audio sync (%d bytes remaining)\n", hdr, isize);
		} else if (version == 1) {
			if (lost_sync) continue; lost_sync = 1;
			printf("    %08x: bad version\n", hdr);
		} else if (layer == 0) {
			if (lost_sync) continue; lost_sync = 1;
			printf("    %08x: bad layer\n", hdr);
		} else if (emphasis == 2) {
			if (lost_sync) continue; lost_sync = 1;
			printf("    %08x: bad emphasis\n", hdr);
		} else if (samplerate == 3) {
			if (lost_sync) continue; lost_sync = 1;
			printf("    %08x: bad samplerate\n", hdr);
		} else {
			layerval = layervals[layer];
			if (version == 0x3) {
				brx = layerval - 1;
			} else if (version == 2 && layerval == 1) {
				brx = 3;
			} else {
				brx = 4;
			}
			bitrate_val = bitrates[bitrate][brx];
			if (bitrate_val <= 0) {
				if (lost_sync) continue; lost_sync = 1;
				printf("    %08x: Bad bitrate\n", hdr);
			} else if (hdrtype && (hdr >> 17) != hdrtype) {
				if (lost_sync) continue; lost_sync = 1;
				printf("    %08x: bad header type (expected %08x)\n", hdr, hdrtype<<17);
			} else {
				const unsigned int srx_vals[4] = {2, -1, 1, 0};
				unsigned int nchannels, srx = srx_vals[version];
				hdrtype = hdr >> 17;
				nchannels = (channelmode == 3) ? 1 : 2;
				if (layerval == 1) {
					flen = (12000 * bitrate_val / samplerates[samplerate][srx] + padding) * 2 * nchannels;
				} else {
					flen = nchannels * 72000 * bitrate_val / samplerates[samplerate][srx] + padding;
				}
				if ((flen - 1) > isize) {
					if (lost_sync)
						continue;
					lost_sync = 1;
					printf("    %08x: bad framelength (%d, but only %d bytes remaining)\n", hdr, flen, isize+1);
				} else {
					const char *jsext;
					lost_sync = 0;
					++framecount;
					ibuf  += 3;
					isize -= 3;
					/*
					 * Keep track of frame offsets for use in recalculating Xing header later
					 */
					if (framecount >= foffsets_count) {
						foffsets_count += (isize / 313) + 100;
						foffsets = realloc(foffsets, foffsets_count * sizeof(unsigned int));
						if (foffsets == NULL) {
							fprintf(stderr, "Out of memory\n");
							exit(ENOMEM);
						}
					}
					foffsets[framecount] = foffset;
					music_bytes = foffset + flen;

					if (channelmode == 1)
						jsext = jsextensions[layerval == 3][jsextension];
					else
						jsext = "";
					if (config_mp3hdrs) {
						printf("%5d: foffset=%10d, FrameLength=%3d, %s, ",
							framecount, foffset, flen, protections[protection]);
						if (private)
							printf("Private, ");
						if (copyright)
							printf("Copyrighted ");
						printf("%s", original ? "Original" : "Copy");
						printf(", MPEG %s layer %s", 
							versions[version], layers[layer]);
						printf(", %3dkbit/s, %d Hz %s%s",
							bitrate_val, samplerates[samplerate][srx],
							channelmodes[channelmode], jsext);
						if (emphasis)
							printf(", %s", emphasiss[emphasis]);
						printf("\n");
					}
					if (framecount > 1 && bitrate_val != kbps) {
						if (Xing_header == NULL && !file_is_vbr)
							printf("    Xing VBR Header is missing\n");
						file_is_vbr = 1;
					}
					kbps_total += kbps = bitrate_val;
					if (framecount == 1) { /* Check for Xing VBR header and TOC */
						unsigned int Xoffset;
						unsigned char *Xing;
						if ((version & 1) == 1)
							Xoffset = (nchannels == 1) ? 17 : 32;
						else
							Xoffset = (nchannels == 1) ?  9 : 17;
						if (strncmp(ibuf + Xoffset, "Xing", 4)) {
							int i;
							for (i = 0; i < flen; ++i) {
								if (!strncmp(ibuf+i, "Xing", 4)) {
									printf("    Found Xing VBR Header at WRONG offset (%d, should be %d)\n", i, Xoffset);
									Xoffset = i;
									break;
								}
							}
						}
						if (!strncmp(ibuf + Xoffset, "Xing", 4)) {
							int d;

							Xing = ibuf + Xoffset;
							Xing_header = Xing;
							Xing += 4;
							Xing = get4bytes(Xing, &Xflags);
							if ((Xflags & 1))
								Xing = get4bytes(Xing, &Xframes);
							if ((Xflags & 2))
								Xing = get4bytes(Xing, &Xbytes);
							if ((Xflags & 4)) {
								Xtoc = Xing;
								Xing += 100;
							}
							if ((Xflags & 8))
								Xing = get4bytes(Xing, &Xscale);
							d = (ibuf + flen - 4) - Xing;
							if (d < 0) {
								printf("    Xing VBR Header: size mismatch - not enough bytes in frame (%d)\n", d);
							} else if (d > 0 && config_mp3hdrs) {
								printf("    Xing VBR Header: %d excess bytes in frame (harmless)\n", d);
							}
						}
					}
					ibuf  += flen - 4;
					isize -= flen - 4;
				}
			}
		}
	} /* while() */
	foffset = start_offset + isize + ibuf - start;	/* should give total file size in bytes */
	if (framecount)
		printf("    %s rate is %dkbit/s\n", file_is_vbr ? "VBR average" : "bit", kbps_total / framecount);

	if (Xing_header) {
		music_bytes -= foffsets[1];
		Xing_is_bad = dump_Xing("Xing VBR Header", Xing_header, framecount, music_bytes, config_mp3hdrs);
#ifndef NOT_PARANOID
		if ((Xflags & 4) && music_bytes) {
			int f, prev_percent = -1;
			for (f = 2; f <= framecount; ++f) {
				int percent = 100 * (f - 1) / framecount;
				while (prev_percent < percent && prev_percent < 99) {
					unsigned int xtoc = Xtoc[++prev_percent];
					unsigned int val = (int)( (256.0 * foffsets[f] + music_bytes / 2.0) / music_bytes );
					if (val > 255) val = 255;
					if (xtoc != val && xtoc != (val - 1) && xtoc != (val + 1)) {
						Xing_is_bad = 5;
						break;
					}
				}
			}
			for ( ; prev_percent < 99; ++prev_percent) {
				if (Xtoc[prev_percent+1] != Xtoc[prev_percent]) {
					Xing_is_bad = 6;
					break;
				}
			}
		}
#endif
	}
	if (!Xing_is_bad) {
		if (config_repair && config_rewrite)
			--config_rewrite;
	} else if (!config_repair) {
		printf("    Xing VBR header is bad(%d): use the '--repair' flag to fix it.\n", Xing_is_bad);
	} else {
		unsigned char *Xing = Xing_header + 8;
		Xframes = framecount - 1;
		if ((Xflags & 1))
			Xing = put4bytes(Xframes, Xing);
		Xbytes = music_bytes;
		if ((Xflags & 2))
			Xing = put4bytes(Xbytes, Xing);
		if ((Xflags & 4) && music_bytes) {
			int f, prev_percent = -1;
			for (f = 2; f <= framecount; ++f) {
				int percent = 100 * (f - 1) / framecount;
				while (prev_percent < percent && prev_percent < 99) {
					unsigned int val = (int)( (256.0 * foffsets[f] + music_bytes / 2.0) / music_bytes );
					if (val > 255) val = 255;
					Xtoc[++prev_percent] = val;
				}
			}
			for ( ; prev_percent < 99; ++prev_percent) {
				Xtoc[prev_percent+1] = Xtoc[prev_percent];
			}
			Xing += 100;
		}
		if ((Xflags & 8)) {
			Xing = put4bytes(Xscale, Xing);
		}
		(void) dump_Xing("Repaired Xing VBR Header", Xing_header, framecount, music_bytes, 1);
	}
}

static unsigned char *ibuf  = NULL;
static int            isize = 0;

static unsigned char *iRead (int bytes, const char *msg)
{
	unsigned char *p = ibuf;

	if (bytes > isize) {
		fprintf(stderr, "iRead(%d,%d,*) failed (%s)\n", bytes, isize, msg);
		exit(EINVAL);
	}
	isize -= bytes;
	ibuf  += bytes;
	return p;
}

void process_mp3_file (const char *mp3_path, FD mp3_fd, int has_mp3_ext)
{
	unsigned char		*hdr, *tag_path = NULL, *ibuf_alloc = NULL, *music_path = NULL, *root_path = NULL;
	unsigned short		flags;
	int			hdr_flags, has_id3v1 = 0;
	FD			tag_fd = FD_FAILED;
	FD			music_fd = FD_FAILED;
	int			tag_size = 0, tag_version;
	struct stat		sbuf;

	memset(&v1tag, 0, sizeof(v1tag));
	memcpy(v1tag.header, "TAG", 3);
	if (has_mp3_ext)
		root_path = strncpy(Malloc(strlen(mp3_path)-3), mp3_path, strlen(mp3_path)-4);
	else
		root_path = strcpy(Malloc(strlen(mp3_path)+1), mp3_path);

	/*
	 * Get size, and then read entire mp3 file into ibuf[]
	 */
	if (getstat(mp3_path, mp3_fd, &sbuf) != 0) {
		int err = errno;
		fprintf(stderr, "fstat() failed: %s\n", strerror(err));
		exit(err);
	}
	ibuf  = ibuf_alloc = Malloc(sbuf.st_size + 128);	/* extra space for a v1tag */
	isize = Read(mp3_fd, ibuf, sbuf.st_size, "ibuf");
	if (isize != sbuf.st_size) {
		fprintf(stderr, "read() returned %d, expected %ld\n", isize, sbuf.st_size);
		exit(EIO);
	}
	close(mp3_fd);

	/*
	 * look for an id3v1 tag
	 */
	if (isize >= 128 && !strncmp(&ibuf[isize - 128], "TAG", 3)) {
		display_printf("    ID3v1 tag is present\n");
		has_id3v1 = 1;
	}

	/*
	 * look for an id3v2 header
	 */
	if (isize < HDR_SIZE || NULL == (hdr = iRead(HDR_SIZE, "tag_hdr"))
	 || hdr[0] != 'I'  || hdr[1] != 'D' || hdr[2] != '3' || hdr[3] == 0xff || hdr[4] == 0xff
	 || ((hdr[6] | hdr[7] | hdr[8] | hdr[9]) & 0x80) != 0)
	{
		display_printf("    ID3v2 tag is not present\n");
		if (config_remove && config_rewrite)
			--config_rewrite;
		ibuf  = ibuf_alloc;
		isize = sbuf.st_size;
		goto copyout;
	}
	display_printf("    ID3v2 tag is present\n");
	tag_version = hdr[3];
	hdr_flags = hdr[5];

	/*
	 * Compute tag_size, and copy entire tag to tag_fd
	 */
	tag_size = syncsafe(&(hdr[6])) + HDR_SIZE;
	if ((hdr_flags & TAG_FOOTER))
		tag_size += HDR_SIZE;	/* allow for a copy of the HDR in the "FOOTER" */

	tag_size -= HDR_SIZE;		/* subtract the HDR we've already processed */
	if (tag_size < 0 || tag_size > isize) {
		fprintf(stderr, "tag_size (%d) > isize (%d) -- bomb\n", tag_size, isize);
		exit(EINVAL);
	}

	if (config_extract) {
		tag_path = strcat(strcpy(Malloc(strlen(root_path)+7), root_path), ".id3v2");
		tag_fd = Creat(tag_path);
		Write(tag_fd, hdr, tag_size + HDR_SIZE, "copytag");
		close(tag_fd);
		free(tag_path);
	}

	/*
	 * Dump the header
	 */
	if (config_headers) {
		printf("    ID3v2.%d.%d tag (%d bytes)", hdr[3], hdr[4], tag_size);
		if (hdr_flags & TAG_UNSYNC)		printf(", Unsync");
		if (hdr_flags & TAG_EXT_HDR)		printf(", Extended");
		if (hdr_flags & TAG_EXPERIMENTAL)	printf(", Experimental");
		if (hdr_flags & TAG_FOOTER)		printf(", Footer");
		printf("\n");
	}

	/*
	 * process the extended header, if present
	 */
	if (hdr_flags & TAG_EXT_HDR) {
		char		*ehdr;
		unsigned int	ext_size;

		ext_size = syncsafe(ibuf);
		if (ext_size < EXT_HDR_SIZE || ext_size > isize) {
			fprintf(stderr, "Bad extended header (size=%d) -- bomb\n", ext_size);
			exit(EINVAL);
		}
		ehdr = iRead(ext_size, "ehdr");
		tag_size -= ext_size;
		flags = ehdr[5];
		if (config_headers) {
			printf("Extended header, size=%d", ext_size);
			if (flags & EXT_UPDATE)		printf(", Update");
			if (flags & EXT_CRC_DATA)	printf(", Crc32");
			if (flags & EXT_RESTRICTIONS)	printf(", Restrictions");
			printf("\n");
		}
	}

	/*
	 * Process frames until the cat comes home
	 */
	while ((tag_size -= FRAME_HDR_SIZE) >= 0) {
		const char	*fchars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		unsigned int	frame_size, frame_adjust = 0, datalen = 0;
		char		*tbuf, *fhdr, savec;

		fhdr = iRead(FRAME_HDR_SIZE, "fhdr");
		frame_size = (tag_version < 4) ? ntohl(*(int *)&fhdr[4]) : syncsafe(&fhdr[4]);
		if (frame_size < 1 || !fhdr[0] || !fhdr[1] || !fhdr[2] || !fhdr[3])
			break;
		if (!(strchr(fchars,fhdr[0]) && strchr(fchars,fhdr[1]) && strchr(fchars,fhdr[2]) && strchr(fchars,fhdr[3])))
			break;
		flags = (fhdr[4] << 8) + fhdr[5];
		if (config_headers) {
			printf("    Frame ID=%c%c%c%c, size=%4d", fhdr[0], fhdr[1], fhdr[2], fhdr[3], frame_size);
			printf((flags & FRAME_ALTER)      ? ", FrameDiscard" : ", FramePreserve");
			printf((flags & FRAME_FILE_ALTER) ? ", FileDiscard"  : ", FilePreserve");
			if (flags & FRAME_READ_ONLY)
				printf(", ReadOnly");
		}
		if (flags & FRAME_GROUP_INFO) {
			unsigned char groupid = *iRead(1, "groupid");
			frame_adjust += 1;
			headers_printf(", Group=%d", groupid);
		}
		if (flags & FRAME_COMPRESSED) {
			headers_printf(", Compressed");
		}
		if (flags & FRAME_ENCRYPTED) {
			unsigned char encmethod = *iRead(1, "encmethod");
			frame_adjust += 1;
			headers_printf(", EncryptMethod=%d", encmethod);
		}
		if (flags & FRAME_UNSYNC) {
			headers_printf(", UnSynch");
		}
		if (flags & FRAME_DATALEN) {
			(void)get4bytes(ibuf, &datalen);
			(void)iRead(4, "datalen");
			frame_adjust += 4;
			datalen = (tag_version < 4) ? ntohl(datalen) : syncsafe(&datalen);
			headers_printf(", DataLen=%d", datalen);
		}
		headers_printf("\n");
		if (flags & FRAME_DATALEN) {
			frame_size = datalen;
		} else if ((flags & (FRAME_COMPRESSED | FRAME_ENCRYPTED))) {
			printf("    Unimplemented flags (0x%04x) -- skipping to end of ID3 tag\n", flags);
			break;
		} else if ((hdr_flags & TAG_UNSYNC) || (flags & FRAME_UNSYNC)) {
			printf("    UnSynch not implemented -- skipping to end of ID3 tag\n");
			break;
		}
		frame_size -= frame_adjust;
		if (frame_size > tag_size) {
			printf("    frame_size (%d) > tag_size (%d) -- skipping remainder of tag\n", frame_size, tag_size);
			break;
		}
		memcpy(tbuf = Malloc(frame_size+1), ibuf, frame_size);
		(void)iRead(frame_size, "frame_size");
		savec = fhdr[4];
		fhdr[4] = '\0';				/* to permit strcmp() on the frame title: BUG: endian-dependant */
		tbuf[frame_size] = '\0';		/* to permit strcpy() on text fields */
		handle_frame(root_path, fhdr, frame_size, tbuf);
		fhdr[4] = savec;
		free(tbuf);
		tag_size -= frame_size;
	}

copyout:
	if (tag_size > 0)
		(void)iRead(tag_size, "copyout");

	process_mp3_headers(ibuf, isize, config_remove ? 0 : ibuf - ibuf_alloc, has_id3v1);

	if (config_rewrite) {
		int isize_adjust = 0;
		/*
		 * Write the the music file
		 */
		music_path = strcat(strcpy(Malloc(strlen(root_path)+10), root_path), ".XXXXXX");
		music_fd = Mkstemp(music_path);
		display_printf("%24s: \"%s\"\n", "File-Name", mp3_path);
		if (*v1tag.song && !has_id3v1) {
			unsigned char *tail = &ibuf[isize];
			memcpy(tail, v1tag.header,  sizeof(v1tag.header));	tail += sizeof(v1tag.header);
			memcpy(tail, v1tag.song,    sizeof(v1tag.song));	tail += sizeof(v1tag.song);
			memcpy(tail, v1tag.artist,  sizeof(v1tag.artist));	tail += sizeof(v1tag.artist);
			memcpy(tail, v1tag.album,   sizeof(v1tag.album));	tail += sizeof(v1tag.album);
			memcpy(tail, v1tag.year,    sizeof(v1tag.year));	tail += sizeof(v1tag.year);
			memcpy(tail, v1tag.comment, sizeof(v1tag.comment));	tail += sizeof(v1tag.comment);
			memcpy(tail, v1tag.tracknr, sizeof(v1tag.tracknr));	tail += sizeof(v1tag.tracknr);
			memcpy(tail, v1tag.genre,   sizeof(v1tag.genre));	/* tail += sizeof(v1tag.genre); */
			isize_adjust = 128;
			printf("    Appended ID3v1 tag to \"%s\"\n", music_path);
		}
		if (!config_remove) {	/* include ID3v2 tags? */
			ibuf  = ibuf_alloc;
			isize = sbuf.st_size;
		}
		Write(music_fd, ibuf, isize + isize_adjust, "copyout");
		close(music_fd);
		if (unlink(mp3_path) == -1) {
			int err = errno;
			fprintf(stderr, "unlink(\"%s\") failed: %s\n", mp3_path, strerror(err));
			exit(err);
		}
		if (rename(music_path, mp3_path) == -1) {
			int err = errno;
			fprintf(stderr, "rename(\"%s\",\"%s\") failed: %s\n", music_path, mp3_path, strerror(err));
			exit(err);
		}
		printf("    Rewrote: \"%s\"\n", mp3_path);
		free(music_path);
	}
	if (root_path)
		free(root_path);
	if (ibuf_alloc)
		free(ibuf_alloc);
	return;
}

static void usage (const char *myname)
{
	fprintf(stderr, "\n%s:  MP3 header manipulation utility.  Version 3.00  July/2005  Mark Lord", myname);
	fprintf(stderr, "\nThis may be useful for dealing with older or buggy .mp3 software.");
	fprintf(stderr, "\nUsage:    %s  [options]  infile.mp3  [infile2.mp3..]\n", myname);
	fprintf(stderr, "\nOptions:  --display     Display of ID3v2 tag info (default)");
	fprintf(stderr, "\n          --headers     Display ID3v2 frame headers");
	fprintf(stderr, "\n          --mp3hdrs     Display mp3 audio headers");
	fprintf(stderr, "\n          --extract     Copy raw ID3v2 tags into .id3v2 files");
	fprintf(stderr, "\n          --remove      Rewrite .mp3 files, discarding ID3v2 tags");
	fprintf(stderr, "\n          --repair      Rewrite .mp3 files, repairing broken VBR headers");
	fprintf(stderr, "\n          --split       Same as '--extract --remove --repair' options combined");
	fprintf(stderr, "\n          --datafiles   Extract embedded text/image files from ID3v2 tags\n");
	fprintf(stderr, "\nAny and all output files are written to the same directory as the original file.");
	fprintf(stderr, "\nWhen using --remove on a file, the ID3v2 info is used to append an ID3v1 tag"); 
	fprintf(stderr, "\nto the end of the file, if an ID3v1 tag was not already present there.\n\n"); 
}

int main (int argc, char *argv[])
{
	FD		mp3_fd;
	int		option_given = 0, file_given = 0;
	const char	*myname = *argv++;

	if (strrchr(myname, '/') != NULL)
		myname = strrchr(myname, '/') + 1;
	if (--argc < 1) {
		usage(myname);
		exit(0);
	}
	while (argc-- > 0) {
		char *arg = *argv++;
		datafile_count = 0;
		if (!strncmp(arg, "--", 2)) {
			option_given = 1;
			       if (!strcmp(arg, "--headers"  )) { config_headers   = 1;
			} else if (!strcmp(arg, "--mp3hdrs"  )) { config_mp3hdrs   = 1;
			} else if (!strcmp(arg, "--extract"  )) { config_extract   = 1;
			} else if (!strcmp(arg, "--remove"   )) { config_remove    = 1;
			                                          config_repair    = 1;
			} else if (!strcmp(arg, "--repair"   )) { config_repair    = 1;
			} else if (!strcmp(arg, "--split"    )) { config_extract   = 1;
		                                          	  config_remove    = 1;
		                                          	  config_repair    = 1;
			} else if (!strcmp(arg, "--datafiles")) { config_datafiles = 1;
			} else if (!strcmp(arg, "--display"))   { config_display   = 1;
			} else {
				fprintf(stderr, "Unknown flag: %s\n", arg);
				exit(EINVAL);
			}
		} else {
			char *mp3_path = arg;
			int has_mp3_ext = 1;
			if (strlen(arg) < 5 || strcmp(arg + strlen(arg) - 4, ".mp3")) {
				fprintf(stderr, "WARNING: Extension not '.mp3': \"%s\"\n", arg);
				has_mp3_ext = 0;
			}
			if (!option_given)
				config_display = 1;
			file_given = 1;
			mp3_fd = open(mp3_path, O_RDONLY|O_EXCL);
			if (mp3_fd == FD_FAILED) {
				fprintf(stderr, "open(\"%s\") failed: %s; (skipped)\n", mp3_path, strerror(errno));
			} else {
				config_rewrite = config_remove + config_repair;
				printf("Processing \"%s\"\n", mp3_path);
				process_mp3_file(mp3_path, mp3_fd, has_mp3_ext);
			}
		}
	}
	if (!file_given) {
		fprintf(stderr, "No input files given after last option\n");
		exit(EINVAL);
	}
	exit(0);
}

