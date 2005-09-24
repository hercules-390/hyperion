#include "hstdinc.h"
#include "hercules.h"
#include "fillfnam.h"

char *filterarray;

int filter(const struct dirent *ent) {
  if (filterarray == NULL)
    return(1);
  if (strncmp(ent->d_name, filterarray, strlen(filterarray)) == 0)
    return(1);
  return(0);
}

/* tab can be pressed anywhere in the command line, so I have to split
   command line
   for example: attach 0a00 3390 volu_ sf=volume1.shadow
   will be divided
   part1 = "attach 0a00 "
   part2 = "volu"
   part3 = " sf=volume1.shadow"
   only part2 can be changed and it must be divided into path and filename
*/
int tab_pressed(char *cmdlinefull, int *cmdoffset) {
  struct dirent **namelist;
  int n, i, j, len, len1, len2;
  int cmdoff = *(cmdoffset); /* for easy reading of source code */
  char *part1, *part2, *part3;
  char *buff;
  char *filename, *path, *tmp;
  char result[1024];
  BYTE pathname[MAX_PATH];

  /* part3 goes from cursor position to the end of line */
  part3 = cmdlinefull + cmdoff;
  /* looking for ' ','@' or '=' backward, starting from cursor offset
     I am not aware of any other delimiters which can be used in hercules */
  for (i = cmdoff-1; i>=0; i--)
    if (cmdlinefull[i] == ' '
       || cmdlinefull[i] == '@'
       || cmdlinefull[i] == '=')
      break;
  /* part1 is from beginning to found delimiter (including delimiter itself) */
  part1 = (char*) malloc(i+2);
  strncpy(part1, cmdlinefull, i+1);
  part1[i+1]= '\0';

  /* part2 is the interesting one */
  part2 = (char*)malloc(cmdoff - i);
  strncpy(part2, cmdlinefull + i + 1, cmdoff - i - 1);
  part2[cmdoff - i - 1] = '\0';

  path = (char*)malloc(strlen(part2) + 1);
  filename = part2;
  /* is it pure filename or is there whole path ? */
  tmp = strrchr(part2, '/');
  if (tmp != NULL) {
    filename = tmp + 1;
    strncpy(path, part2, strlen(part2)-strlen(filename));
    path[strlen(part2)-strlen(filename)] = '\0';
    tmp[0] = '\0';
  }
  else {
    strcpy(path,"./");
  }
  /* this is used in filter function to include only relevant filenames */
  filterarray = filename;

  n = scandir(path, &namelist, filter, alphasort);
  if (n > 0) {
    for (i=0; i<n; i++) {
      struct STAT buf;
      char fullfilename[256];
      /* namelist[i]->d_name contains filtered filenames, check if they are
         directories with stat(), before that create whole path */
      if (tmp != NULL)
         sprintf(fullfilename, "%s%s", path, namelist[i]->d_name);
      else
         sprintf(fullfilename, "%s", namelist[i]->d_name);
      /* if it is a directory, add '/' to the end so it can be seen on screen*/
      hostpath(pathname, fullfilename, sizeof(pathname));
      if (STAT(pathname,&buf) == 0)
         if (buf.st_mode & S_IFDIR) {
            strcat(namelist[i]->d_name,"/");
         }
    }
    /* now check, if filenames have something in common, after a cycle buff
       contains maximal intersection of names */
    buff = (char*)malloc(strlen(namelist[0]->d_name) + 1); /* first one */
    strcpy(buff, namelist[0]->d_name);
    for (i = 1; i < n; i++) {
      len1 = strlen(buff);
      len2 = strlen(namelist[i]->d_name);
      /* check number of characters in shorter one */
      len = len1 > len2 ? len2 : len1;
      for (j = 0; j < len; j++)
      if (buff[j] != namelist[i]->d_name[j]) {
        buff[j] = '\0'; /* end the string where characters are not equal */
        break;
      }
    }
    /* if maximal intersection of filenames is longer then original filename */
    if (strlen(buff) > strlen(filename)) {
      char *fullfilename;

      fullfilename = (char*)malloc(strlen(path) + strlen(buff) + 1);
      /* this test is not useless as path contains './' if there was no path
         in original filename. it is because of scandir function, which
         needs path portion */
      if (tmp != NULL)
         sprintf(fullfilename, "%s%s", path, buff);
      else
         sprintf(fullfilename, "%s", buff);
      /* construct command line */
      sprintf(result, "%s%s%s", part1, fullfilename, part3);
      /* move cursor */
      *(cmdoffset) = strlen(part1) + strlen(fullfilename);
      strcpy(cmdlinefull, result);
      free(fullfilename);
    }
    else {
      /* display all alternatives */
      for (i = 0; i< n; i++)
         logmsg("%s\n", namelist[i]->d_name);
      logmsg("\n");
    }
    /* free everything */
    free(buff);
    for (i = 0; i< n; i++)
      free(namelist[i]);
    free(namelist);

  }
  free(part1);
  free(part2);
  free(path);
  return(0);
}
