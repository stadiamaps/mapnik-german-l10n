/*

Thai to Latin transcription code via Thai language processing package
https://pypi.org/project/tltk/

Hopefully using Royal Thai General System of Transcription (RTGS)

(c) 2018-2019 Sven Geggus <svn-osm@geggus.net>

*/

CREATE or REPLACE FUNCTION osml10n_thai_transcript(inpstr text) RETURNS TEXT AS $$
  import unicodedata
  import plpy

  def split_by_alphabet(str):
    strlist=[]
    target=''
    oldalphabet=unicodedata.name(str[0]).split(' ')[0]
    target=str[0]
    for c in str[1:]:
      alphabet=unicodedata.name(c).split(' ')[0]
      if (alphabet==oldalphabet):
        target=target+c
      else:
        strlist.append(target)
        target=c
      oldalphabet=alphabet
    strlist.append(target)
    return(strlist)

  # Delicate dance. Importing twice allows us to ignore an error when gensim isn't installed.
  try:
    from tltk.nlp import th2roman
  except:
    try:
      from tltk.nlp import th2roman
    except:
      plpy.notice("tltk not installed, falling back to ICU")
      return(None)
    
  try:
    stlist=split_by_alphabet(inpstr)
  except:
    plpy.notice("tltk error transcribing >%s<" % inpstr)
    return(None)

  latin = ''
  for st in stlist:
    if (unicodedata.name(st[0]).split(' ')[0] == 'THAI'):
      transcript=''
      try:
        transcript=th2roman(st).rstrip('<s/>').rstrip()
      except:
        plpy.notice("tltk error transcribing >%s<" % st)
        return(None)
      latin=latin+transcript
    else:
      latin=latin+st
  return(latin)
$$ LANGUAGE plpython3u STABLE;
