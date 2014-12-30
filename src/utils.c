#include "RS-MySQL.h"


/* Make a data.frame from a named list by adding row.names, and class
* attribute.  Use "1", "2", .. as row.names.
* NOTE: Only tested  under R (not tested at all under S4 or Splus5+).
*/
void RS_DBI_makeDataFrame(SEXP data) {
  SEXP row_names, df_class_name;
  int   i, n;
  char   buf[1024];

  PROTECT(data);
  PROTECT(df_class_name = NEW_CHARACTER((int) 1));
  SET_CHR_EL(df_class_name, 0, C_S_CPY("data.frame"));

  /* row.names */
  n = GET_LENGTH(LST_EL(data,0));            /* length(data[[1]]) */
  PROTECT(row_names = NEW_CHARACTER(n));
  for(i=0; i<n; i++){
    (void) sprintf(buf, "%d", i+1);
    SET_CHR_EL(row_names, i, C_S_CPY(buf));
  }

  setAttrib(data, R_RowNamesSymbol, row_names);
  SET_CLASS(data, df_class_name);

  UNPROTECT(3);
  return;
}

void RS_DBI_allocOutput(SEXP output, RS_DBI_fields *flds, int num_rec, int  expand) {
  SEXP names, s_tmp;
  int   j;
  int    num_fields;
  SEXPTYPE  *fld_Sclass;

  PROTECT(output);

  num_fields = flds->num_fields;
  if(expand){
    for(j = 0; j < (int) num_fields; j++){
      /* Note that in R-1.2.3 (at least) we need to protect SET_LENGTH */
      s_tmp = LST_EL(output,j);
      PROTECT(SET_LENGTH(s_tmp, num_rec));
      SET_ELEMENT(output, j, s_tmp);
      UNPROTECT(1);
    }
    UNPROTECT(1);
    return;
  }

  fld_Sclass = flds->Sclass;
  for(j = 0; j < (int) num_fields; j++){
    switch((int)fld_Sclass[j]){
    case LGLSXP:
      SET_ELEMENT(output, j, NEW_LOGICAL(num_rec));
      break;
    case STRSXP:
      SET_ELEMENT(output, j, NEW_CHARACTER(num_rec));
      break;
    case INTSXP:
      SET_ELEMENT(output, j, NEW_INTEGER(num_rec));
      break;
    case REALSXP:
      SET_ELEMENT(output, j, NEW_NUMERIC(num_rec));
      break;
    case VECSXP:
      SET_ELEMENT(output, j, NEW_LIST(num_rec));
      break;
    default:
      RS_DBI_errorMessage("unsupported data type", RS_DBI_ERROR);
    }
  }

  PROTECT(names = NEW_CHARACTER((int) num_fields));
  for(j = 0; j< (int) num_fields; j++){
    SET_CHR_EL(names,j, C_S_CPY(flds->name[j]));
  }
  SET_NAMES(output, names);

  UNPROTECT(2);

  return;
}


/* wrapper to strcpy */
char* RS_DBI_copyString(const char *str) {
  char *buffer;

  buffer = (char *) malloc((size_t) strlen(str)+1);
  if(!buffer)
    RS_DBI_errorMessage(
      "internal error in RS_DBI_copyString: could not alloc string space",
      RS_DBI_ERROR);
  return strcpy(buffer, str);
}

/* wrapper to strncpy, plus (optionally) deleting trailing spaces */
char* RS_DBI_nCopyString(const char *str, size_t len, int del_blanks) {
  char *str_buffer, *end;

  str_buffer = (char *) malloc(len+1);
  if(!str_buffer){
    char errMsg[128];
    (void) sprintf(errMsg,
      "could not malloc %ld bytes in RS_DBI_nCopyString",
      (long) len+1);
    RS_DBI_errorMessage(errMsg, RS_DBI_ERROR);
  }
  if(len==0){
    *str_buffer = '\0';
    return str_buffer;
  }

  (void) strncpy(str_buffer, str, len);

  /* null terminate string whether we delete trailing blanks or not*/
  if(del_blanks){
    for(end = str_buffer+len-1; end>=str_buffer; end--)
      if(*end != ' ') { end++; break; }
      *end = '\0';
  }
  else {
    end = str_buffer + len;
    *end = '\0';
  }
  return str_buffer;
}

SEXP RS_DBI_createNamedList(char **names, SEXPTYPE *types, int *lengths, int  n) {
  SEXP output, output_names, obj = R_NilValue;
  int  num_elem;
  int   j;

  PROTECT(output = NEW_LIST(n));
  PROTECT(output_names = NEW_CHARACTER(n));
  for(j = 0; j < n; j++){
    num_elem = lengths[j];
    switch((int)types[j]){
    case LGLSXP:
      PROTECT(obj = NEW_LOGICAL(num_elem));
      break;
    case INTSXP:
      PROTECT(obj = NEW_INTEGER(num_elem));
      break;
    case REALSXP:
      PROTECT(obj = NEW_NUMERIC(num_elem));
      break;
    case STRSXP:
      PROTECT(obj = NEW_CHARACTER(num_elem));
      break;
    case VECSXP:
      PROTECT(obj = NEW_LIST(num_elem));
      break;
    default:
      RS_DBI_errorMessage("unsupported data type", RS_DBI_ERROR);
    }
    SET_ELEMENT(output, (int)j, obj);
    SET_CHR_EL(output_names, j, C_S_CPY(names[j]));
  }
  SET_NAMES(output, output_names);
  UNPROTECT(n+2);
  return(output);
}

/* Very simple objectId (mapping) table. newEntry() returns an index
 * to an empty cell in table, and lookup() returns the position in the
 * table of obj_id.  Notice that we decided not to touch the entries
 * themselves to give total control to the invoking functions (this
 * simplify error management in the invoking routines.)
 */
int RS_DBI_newEntry(int *table, int length)   {
  int i, indx, empty_val;

  indx = empty_val = (int) -1;
  for(i = 0; i < length; i++)
    if(table[i] == empty_val){
      indx = i;
      break;
    }
    return indx;
}
int RS_DBI_lookup(int *table, int length, int obj_id) {
  int i, indx;

  indx = (int) -1;
  for(i = 0; i < length; ++i){
    if(table[i]==obj_id){
      indx = i;
      break;
    }
  }
  return indx;
}

/* return a list of entries pointed by *entries (we allocate the space,
 * but the caller should free() it).  The function returns the number
 * of entries.
 */
int RS_DBI_listEntries(int *table, int length, int *entries)   {
  int i,n;

  for(i=n=0; i<length; i++){
    if(table[i]<0) continue;
    entries[n++] = table[i];
  }
  return n;
}
void RS_DBI_freeEntry(int *table, int indx) { /* no error checking!!! */
int empty_val = (int) -1;
  table[indx] = empty_val;
  return;
}


/*  These 2 R-specific functions are used by the C macros IS_NA(p,t)
*  and NA_SET(p,t) (in this way one simply use macros to test and set
*  NA's regardless whether we're using R or S.
*/
void
  RS_na_set(void *ptr, SEXPTYPE type)
  {
    double *d;
    int   *i;
    switch(type){
    case INTSXP:
      i = (int *) ptr;
      *i = NA_INTEGER;
      break;
    case LGLSXP:
      i = (int *) ptr;
      *i = NA_LOGICAL;
      break;
    case REALSXP:
      d = (double *) ptr;
      *d = NA_REAL;
      break;
    }
  }
int
  RS_is_na(void *ptr, SEXPTYPE type)
  {
    int *i, out = -2;
    char *c;
    double *d;

    switch(type){
    case INTSXP:
    case LGLSXP:
      i = (int *) ptr;
      out = (int) ((*i) == NA_INTEGER);
      break;
    case REALSXP:
      d = (double *) ptr;
      out = ISNA(*d);
      break;
    case STRSXP:
      c = (char *) ptr;
      out = (int) (strcmp(c, CHR_EL(NA_STRING, 0))==0);
      break;
    }
    return out;
  }


/* The single string version of this function was kindly provided by
* J. T. Lindgren (any bugs are probably dj's)
*
* NOTE/BUG?: This function could potentially grab a huge amount of memory
*   if given (not inappropriately) very large binary objects. How should
*   we protect against potentially deadly requests?
*/

SEXP RS_MySQL_escapeStrings(SEXP conHandle, SEXP strings)
{
  RS_DBI_connection *con;
  MYSQL             *my_connection;
  long len, old_len;
  int i, nStrings;
  char *str;
  char *escapedString;
  SEXP output;

  con = RS_DBI_getConnection(conHandle);
  my_connection = (MYSQL *) con->drvConnection;

  nStrings = GET_LENGTH(strings);
  PROTECT(output = NEW_CHARACTER(nStrings));

  old_len = (long) 1;
  escapedString = (char *) S_alloc(old_len, (int) sizeof(char));
  if(!escapedString){
    RS_DBI_errorMessage(
      "(RS_MySQL_escapeStrings) could not allocate memory",
      RS_DBI_ERROR);
  }

  for(i=0; i<nStrings; i++){
    str = RS_DBI_copyString(CHR_EL(strings,i));
    len = (long) strlen(str);
    escapedString = (char *) S_realloc(escapedString,
      (long) 2*len+1, old_len, (int)sizeof(char));
    if(!escapedString){
      RS_DBI_errorMessage(
        "(RS_MySQL_escapeStrings) could not (re)allocate memory",
        RS_DBI_ERROR);
    }

    mysql_real_escape_string(my_connection, escapedString, str, len);

    SET_CHR_EL(output, i, C_S_CPY(escapedString));
  }

  UNPROTECT(1);
  return output;
}

SEXP
  RS_MySQL_clientLibraryVersions(void)
  {
    SEXP ret, name;

    PROTECT(name=NEW_CHARACTER(2));
    SET_STRING_ELT(name, 0, COPY_TO_USER_STRING(MYSQL_SERVER_VERSION));
    SET_STRING_ELT(name, 1, COPY_TO_USER_STRING(mysql_get_client_info()));
    PROTECT(ret=NEW_INTEGER(2));
    INTEGER(ret)[0] = (int)MYSQL_VERSION_ID;
    INTEGER(ret)[1] = (int)mysql_get_client_version();
    SET_NAMES(ret,name);
    UNPROTECT(2);
    return ret;
  }

void RS_DBI_errorMessage(char *msg, DBI_EXCEPTION exception_type) {
  char *driver = "RS-DBI";   /* TODO: use the actual driver name */

switch(exception_type) {
case RS_DBI_MESSAGE:
  warning("%s driver message: (%s)", driver, msg);
  break;
case RS_DBI_WARNING:
  warning("%s driver warning: (%s)", driver, msg);
  break;
case RS_DBI_ERROR:
  error("%s driver: (%s)", driver, msg);
  break;
case RS_DBI_TERMINATE:
  error("%s driver fatal: (%s)", driver, msg); /* was TERMINATE */
break;
}
return;
}
