/*
 
Load using the following command:
CREATE FUNCTION osml10n_kanji_transcript(text) RETURNS text
AS '/path/to/osml10n_kanjitranscript.so', 'osml10n_kanji_transcript' LANGUAGE C STRICT;

(c) 2015 Sven Geggus <sven-osm@geggus.net>

Licence AGPL http://www.gnu.org/licenses/agpl-3.0.de.html

*/
#include <postgres.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libkakasi.h>
#include <mb/pg_wchar.h>
#include <fmgr.h>
#include <utf8proc.h>
#include "varatt.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(osml10n_kanji_transcript);

Datum osml10n_kanji_transcript(PG_FUNCTION_ARGS) {
  char *inbuf;
  char *normalized;
  char *kakasi_out;
  // Invoke kakasi in UTF-8 mode. The older EUC-JP path misromanized some
  // characters (e.g. the kokuji 糀) and leaked EUC-JP lead bytes into the
  // output, producing invalid UTF-8 downstream.
  char *kakasi_argv[8]={"kakasi","-iutf8","-outf8","-Ja","-Ha","-Ka","-Ea","-s"};

  if (GetDatabaseEncoding() != PG_UTF8) {
    ereport(ERROR,(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
    errmsg("requires UTF8 database encoding")));
    PG_RETURN_NULL();
  }

  text *t = PG_GETARG_TEXT_P(0);

  inbuf=(char *) malloc((VARSIZE(t) - VARHDRSZ +1)*sizeof(char));
  memcpy(inbuf, (void *) VARDATA(t), VARSIZE(t) - VARHDRSZ);
  inbuf[VARSIZE(t) - VARHDRSZ]='\0';

  // Use Normalization Form KC to fold compatibility characters like
  // ㈱ PARENTHESIZED IDEOGRAPH STOCK into their base forms before romanizing.
  // https://en.wikipedia.org/wiki/Enclosed_CJK_Letters_and_Months
  normalized=utf8proc_NFKC(inbuf);
  if (NULL == normalized) {
    ereport(ERROR, (errmsg("error calling utf8proc_NFKC")));
    free(inbuf);
    PG_RETURN_NULL();
  }
  free(inbuf);

  kakasi_getopt_argv(8,kakasi_argv);
  kakasi_out=kakasi_do(normalized);
  if (kakasi_out==NULL) {
    free(normalized);
    ereport(ERROR, (errmsg("kakasi_do failed")));
    PG_RETURN_NULL();
  }

  int32_t obufLen = strlen(kakasi_out);

  if (!pg_verify_mbstr(GetDatabaseEncoding(), kakasi_out, obufLen, true)) {
    ereport(NOTICE,
            (errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
             errmsg("kakasi error transcribing >%s<", normalized)));
    kakasi_free(kakasi_out);
    free(normalized);
    PG_RETURN_NULL();
  }
  free(normalized);

  text *new_text = (text *) palloc(VARHDRSZ + obufLen);
  SET_VARSIZE(new_text, VARHDRSZ + obufLen);
  memcpy((void *) VARDATA(new_text), /* destination */
         (void *) kakasi_out,obufLen);
  kakasi_free(kakasi_out);
  PG_RETURN_TEXT_P(new_text);
}
