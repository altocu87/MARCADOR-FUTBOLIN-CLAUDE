// Notas de la próxima versión: buffers acotados, sin lista dinámica en RAM.
#ifndef MFV3_S3_UPDATE_INFO_H
#define MFV3_S3_UPDATE_INFO_H
#include <stddef.h>
#include <string.h>
namespace updateInfo {
struct Release { char version[16] = {}; char notes[1024] = {}; };
// Un build mayor de otra rama no debe reemplazar una versión de producto más nueva.
inline bool versionParts(const char* text,unsigned out[3]){
  if(!text || !*text)return false;
  for(unsigned i=0;i<3;i++){
    out[i]=0;if(!*text)return i>=2;
    unsigned digits=0;
    while(*text>='0' && *text<='9'){if(++digits>3)return false;out[i]=out[i]*10+(*text++-'0');}
    if(!digits)return false;
    if(!*text){for(unsigned j=i+1;j<3;j++)out[j]=0;return i>=1;}
    if(*text++!='.' || !*text)return false;
  }
  return !*text;
}
inline bool olderVersion(const char* published,const char* installed){
  unsigned a[3],b[3];if(!versionParts(published,a) || !versionParts(installed,b))return false;
  for(unsigned i=0;i<3;i++)if(a[i]!=b[i])return a[i]<b[i];
  return false;
}
inline bool readString(const char* json, const char* key, char* out, size_t cap) {
  out[0] = 0;
  const char* p = mfv3::ota::findKey(json, key);
  if (!p || *p++ != '"') return false;
  size_t n = 0;
  while (*p && *p != '"') {
    unsigned char c = (unsigned char)*p++;
    if (c == '\\') {
      c = (unsigned char)*p++;
      if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
      else if (c != '"' && c != '\\' && c != '/') { out[0] = 0; return false; }
    } else if (c < 32) { out[0] = 0; return false; }
    if (n + 1 >= cap) { out[0] = 0; return false; }
    out[n++] = (char)c;
  }
  if (*p != '"') { out[0] = 0; return false; }
  out[n] = 0;
  return n != 0;
}
inline void parse(const char* json, Release& out) {
  out = Release();
  if (!readString(json, "version", out.version, sizeof(out.version))) return;
  // Las notas antiguas pueden faltar; la actualización sigue siendo compatible.
  for (const char* p = out.version; *p; ++p) if ((*p < '0' || *p > '9') && *p != '.') { out = Release(); return; }
  readString(json, "notes", out.notes, sizeof(out.notes));
}
}
#endif
