// mp3mobile: read an ID3 tag
// altman@cryton.demon.co.uk
// Read an ID3 tag from a file into a songinfo struct: if there's no ID3,
// make something up with the filename. The songinfo struct just has
// song, artist and year fields, along with other bits.
static int id3_read(char *tune,songinfo *s)
  {
  FILE *f; char buffer[256];

  /* Make pathname */
  sprintf(buffer,"%s%s",base_path,tune);

  /* Try to open the .mp3 */
  if ((f=fopen(buffer,"rb"))==NULL) return(1);

  /* Go to 128 bytes from the end */
  fseek(f,-128L,SEEK_END);

  /* Read the buffer */
  fread(buffer,1,128,f);
  fclose(f);

  /* Look for 'TAG' indicating ID3 tag */
  if (strncmp(buffer,"TAG",3)==0)
    {
    char *b=buffer+3,*c=b+29;

    /* Found tag, read out title & artist */
    while(c>b && *c==' ') c--;
    *++c=0;

    /* Save title */
    strcpy(s->song,b);

    b=buffer+33; c=b+29;
    while(c>b && *c==' ') c--;
    *++c=0;

    /* Save artist */
    strcpy(s->artist,b);

    /* Save year */
    if ((s->year=atoi(buffer+93))==0) s->year=9999;
    }
  else
    {
    char *b;

    /* Take first part of path as band, second as tune */
    strcpy(buffer,tune);

    /* Convert all _'s to spaces */
    b=buffer;
    while(*b) if (*b=='_') *b++=' '; else b++;

    if ((b=strchr(buffer,'/'))==NULL)
      {
      /* No path separator, just use filename as song */
      strcpy(s->song,buffer);
      s->artist[0]=0;
      }
    else
      {
      /* Separate them */
      *b++=0;
      strcpy(s->song,b);
      strcpy(s->artist,buffer);
      }
    }

  return(0);
  }
