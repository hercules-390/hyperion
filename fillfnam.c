// $Id$
//
// $Log$
// Revision 1.11  2008/02/18 22:57:08  rbowler
// Disable filename completion for Solaris 2.9 due to missing scandir function
//
// Revision 1.10  2006/12/31 17:52:50  gsmith
// 31 Dec 2006 Fix typo in line 1 of fillfnam.c
//
// Revision 1.9  2006/12/31 11:31:21  fish
// Fix cmdline tabbed filename-completion for MSVC
//
// Revision 1.8  2006/12/08 09:43:21  jj
// Add CVS message log
//

#include "hstdinc.h"
#include "hercules.h"
#include "fillfnam.h"

/* On Solaris 2.9 (SunOS 5.9) and earlier, there is no scandir
   and alphasort function. In this case fillfnam does nothing
   and the tab command is effectively a no-operation */
#if !(defined(HAVE_SCANDIR) && defined(HAVE_ALPHASORT)) && !defined(_MSVC_)
int tab_pressed(char *cmdlinefull, int *cmdoffset) {
  UNREFERENCED(cmdlinefull);
  UNREFERENCED(cmdoffset);  
  return 0; 
}
#else 

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
  char pathname[MAX_PATH];
#ifdef _MSVC_
  int within_quoted_string = 0;
  int quote_pos;
#endif

  /* part3 goes from cursor position to the end of line */
  part3 = cmdlinefull + cmdoff;
  /* looking for ' ','@' or '=' backward, starting from cursor offset
     I am not aware of any other delimiters which can be used in hercules */
  /* (except for '"' (double quote) for the MSVC version of herc - Fish) */
#ifdef _MSVC_
  /* determine if tab was pressed within a quoted string */
  for (i=0; i < cmdoff; i++)
      if ('\"' == cmdlinefull[i])
      {
          within_quoted_string = !within_quoted_string;
          quote_pos = i; /* (save position of quote
                         immediately preceding cmdoff) */
      }
  if (within_quoted_string)
    i = quote_pos; /* (the quote is our delimiter) */
  else
#endif
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

  len = (int)strlen(part2);
  /* 3 characters minimum needed, for ./\0 in path. */
  /* (or 4 chars for MSVC if within quoted string, for \"./\0 - Fish) */
#ifdef _MSVC_
  if (within_quoted_string)
  {
    if (len < 3)
      len = 3;
  }
  else
#endif
  if (len < 2)
    len = 2;
  path = (char*)malloc(len + 1);
  *path = '\0';
  filename = part2;
  /* is it pure filename or is there whole path ? */
#ifdef _MSVC_
  tmp = strrchr(part2, '\\');
#else
  tmp = strrchr(part2, '/');
#endif
#ifdef _MSVC_
  if (!tmp) tmp = strrchr(part2, '\\');
#endif
  if (tmp != NULL) {
    filename = tmp + 1;
    strncpy(path, part2, strlen(part2)-strlen(filename));
    path[strlen(part2)-strlen(filename)] = '\0';
    tmp[0] = '\0';
  }
  else {
#ifdef _MSVC_
    if (within_quoted_string)
        strcpy(path,"\".\\");
    else
        strcpy(path,".\\");
#else
        strcpy(path,"./");
#endif

  }
  /* this is used in filter function to include only relevant filenames */
  filterarray = filename;

  n = scandir(path, &namelist, filter, alphasort);
  if (n > 0) {
    for (i=0; i<n; i++) {
      struct stat buf;
      char fullfilename[1+MAX_PATH+1];
      /* namelist[i]->d_name contains filtered filenames, check if they are
         directories with stat(), before that create whole path */
      if (tmp != NULL)
         sprintf(fullfilename, "%s%s", path, namelist[i]->d_name);
      else
         sprintf(fullfilename, "%s", namelist[i]->d_name);
#ifdef _MSVC_
      if (within_quoted_string)
        strlcat(fullfilename,"\"",sizeof(fullfilename));
#endif
      /* if it is a directory, add '/' to the end so it can be seen on screen*/
      hostpath(pathname, fullfilename, sizeof(pathname));
      if (stat(pathname,&buf) == 0)
         if (buf.st_mode & S_IFDIR) {
//          strcat(namelist[i]->d_name,"/");
// Don't write past end of d_name
// Problem debugged by bb5ch39t
            namelist[i] = realloc(namelist[i], sizeof(struct dirent)
                                + strlen(namelist[i]->d_name) + 2);
            if (namelist[i])
#ifdef _MSVC_
               strcat(namelist[i]->d_name,"\\");
#else
               strcat(namelist[i]->d_name,"/");
#endif
         }
    }
    /* now check, if filenames have something in common, after a cycle buff
       contains maximal intersection of names */
    buff = (char*)malloc(strlen(namelist[0]->d_name) + 1); /* first one */
    strcpy(buff, namelist[0]->d_name);
    for (i = 1; i < n; i++) {
      len1 = (int)strlen(buff);
      len2 = (int)strlen(namelist[i]->d_name);
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
      *(cmdoffset) = (int)(strlen(part1) + strlen(fullfilename));
      strcpy(cmdlinefull, result);
      free(fullfilename);
    }
    else {
      /* display all alternatives */
      for (i = 0; i< n; i++)
         logmsg("%s\n", namelist[i]->d_name);
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
#endif /*(HAVE_SCANDIR && HAVE_ALPHASORT) || _MSVC_*/
