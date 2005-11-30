/*********************************************************************
 *
 * Authors: Vincenzo Ciaschini - Vincenzo.Ciaschini@cnaf.infn.it 
 *
 * Copyright (c) 2002, 2003 INFN-CNAF on behalf of the EU DataGrid.
 * For license conditions see LICENSE file or
 * http://www.edg.org/license.html
 *
 * Parts of this code may be based upon or even include verbatim pieces,
 * originally written by other people, in which case the original header
 * follows.
 *
 *********************************************************************/

extern "C" {
#include "config.h"
#include "replace.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include "newformat.h"
#include <openssl/bn.h>
#include "credentials.h"

#ifdef HAVE_GLOBUS_MODULE_ACTIVATE
#include <globus_module.h>
#include <globus_openssl.h>
#endif
}

#include <fstream>
#include <iostream>

#include <voms_api.h>
#include "data.h"
#include "vomsxml.h"

extern bool retrieve(X509 *cert, STACK_OF(X509) *chain, recurse_type how, 
		     std::string &buffer, std::string &vo, std::string &file, 
		     std::string &subject, std::string &ca, verror_type &error);
/*
extern bool verify(std::string message, vomsdata &voms, verror_type &error, 
		   std::string vdir, std::string cdir, std::string subject, 
		   std::string ca);
*/

extern "C" {
extern char *Decode(const char *, int, int *);
extern char *Encode(const char *, int, int *);
}

extern int AC_Init(void);
vomsdata::Initializer::Initializer(Initializer &) {}
vomsdata::Initializer::Initializer()
{
#ifdef HAVE_GLOBUS_MODULE_ACTIVATE
  (void)globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE);
  (void)globus_module_activate(GLOBUS_OPENSSL_MODULE);
#endif
  SSLeay_add_all_algorithms();
  (void)AC_Init();
}

vomsdata::Initializer vomsdata::init;

void vomsdata::seterror(verror_type err, std::string message)
{
  error = err;
  errmessage = message;
}

std::string vomsdata::ErrorMessage(void)
{
  return errmessage;
}

vomsdata::vomsdata(std::string voms_dir, std::string cert_dir) :  ca_cert_dir(cert_dir),
                                                                  voms_cert_dir(voms_dir),
                                                                  duration(0),
                                                                  ordering(""),
                                                                  error(VERR_NONE),
                                                                  workvo(""),
                                                                  extra_data(""),
                                                                  ver_type(VERIFY_FULL),
                                                                  noglobus(false)
{
  if (voms_cert_dir.empty()) {
    char *v;
    if ( (v = getenv("X509_VOMS_DIR")))
      voms_cert_dir = std::string(v);
    else 
      voms_cert_dir = "/etc/grid-security/vomsdir";
  }

  if (ca_cert_dir.empty()) {
    char *c;
    if ((c = getenv("X509_CERT_DIR")))
      ca_cert_dir = std::string(c);
    else
      ca_cert_dir = "/etc/grid-security/certificates";
  }

  DIR *vdir, *cdir;
  vdir = opendir(voms_cert_dir.c_str());
  cdir = opendir(ca_cert_dir.c_str());

  if (!vdir)
    seterror(VERR_DIR, "Unable to find vomsdir directory");

  if (!cdir)
    seterror(VERR_DIR, "Unable to find ca certificates");
    
#ifdef HAVE_GLOBUS_MODULE_ACTIVATE
  if (globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE) != GLOBUS_SUCCESS) {
    seterror(VERR_NOINIT, "Unable to initialize globus.");
    noglobus = true;
  }
  if (globus_module_activate(GLOBUS_OPENSSL_MODULE) != GLOBUS_SUCCESS) {
    seterror(VERR_NOINIT, "Unable to initialize globus.");
    globus_module_deactivate(GLOBUS_GSI_GSS_ASSIST_MODULE);
    noglobus = true;
  }
#endif

  if (cdir)
    (void)closedir(cdir);
  if (vdir)
    (void)closedir(vdir);

  duration = 0;
}

vomsdata::~vomsdata()
{
#ifdef HAVE_GLOBUS_MODULE_ACTIVATE
  if (!noglobus) {
    globus_module_deactivate(GLOBUS_GSI_GSS_ASSIST_MODULE);
    globus_module_deactivate(GLOBUS_OPENSSL_MODULE);
  }
#endif
}

std::string vomsdata::ServerErrors(void)
{
  std::string err = serverrors;
  serverrors="";

  return err;
}

void vomsdata::ResetTargets(void)
{
  targets.clear();
}

std::vector<std::string> vomsdata::ListTargets(void)
{
  return targets;
}

void vomsdata::AddTarget(std::string target)
{
  targets.push_back(target);
}

void vomsdata::SetLifetime(int lifetime)
{
  duration = lifetime;
}

void vomsdata::SetVerificationType(verify_type t)
{
  ver_type = t;
}

void vomsdata::ResetOrder(void)
{
  ordering="";
}

void vomsdata::Order(std::string att)
{
  ordering += (ordering.empty() ? "" : ",") + att;
}

bool vomsdata::ContactRaw(std::string hostname, int port, std::string servsubject, std::string command, std::string &raw, int& version)
{
  std::string buffer;
  std::string subject, ca;
  std::string lifetime;

  std::string comm;
  std::string targs;
  answer a;

  for (std::vector<std::string>::iterator i = targets.begin(); 
       i != targets.end(); i++) {
    if (i == targets.begin())
      targs = *i;
    else
      targs += std::string(",") + *i;
  }

  comm = XML_Req_Encode(command, ordering, targs, duration);

  if (!contact(hostname, port, servsubject, comm, buffer, subject, ca))
    return false;
  
  if (XML_Ans_Decode(buffer, a)) {
    bool result = true;
    if (!a.ac.empty()) {
      buffer = a.ac;
      if (a.errs.size() != 0) {
        for (std::vector<errorp>::iterator i = a.errs.begin();
             i != a.errs.end(); i++) {
          serverrors += i->message;
          if (i->num > ERROR_OFFSET)
            result = false;
          if (i->num == WARN_NO_FIRST_SELECT)
            seterror(VERR_ORDER, "Cannot put requested attributes in the specified order.");
        }
      }
    }
    else if (!a.data.empty()) {
      buffer = a.data;
    }
    if (!result && ver_type) {
      seterror(VERR_SERVERCODE, "The server returned an error.");
      return false;
    }
    raw = buffer;
  }
  else {
    seterror(VERR_FORMAT, "Server Answer was incorrectly formatted.");
    return false;
  }

  version = 1;
  return true;
}

bool vomsdata::Contact(std::string hostname, int port, std::string servsubject, std::string command)
{
  std::string subject, ca;
  char *s = NULL, *c = NULL;

  std::string message;
  bool result = false;
  int version;

  if (ContactRaw(hostname, port, servsubject, command, message, version)) {

    X509 *holder = get_own_cert();

    if (holder) {
      error = VERR_NONE;
      c = X509_NAME_oneline(X509_get_issuer_name(holder), NULL,  0);
      s = X509_NAME_oneline(X509_get_subject_name(holder), NULL, 0);
      
      if (c && s) {
        ca = std::string(c);
        subject = std::string(s);
    
        voms v;
  
        result = verifydata(message, subject, ca, holder, v);
	
        if (result)
          data.push_back(v);
      }
      X509_free(holder);
    }
    else
      seterror(VERR_NOIDENT, "Cannot discover own credentials.");
  }
  
  free(c);
  free(s);

  return result;
}

bool vomsdata::RetrieveFromCred(gss_cred_id_t cred, recurse_type how)
{
  X509 *cert;
  STACK_OF(X509) *chain;

  cert = decouple_cred(cred, 0, &chain);

  return Retrieve(cert, chain, how);
}

bool vomsdata::RetrieveFromCtx(gss_ctx_id_t cred, recurse_type how)
{
  X509 *cert;
  STACK_OF(X509) *chain;

  cert = decouple_ctx(cred, 0, &chain);

  return Retrieve(cert, chain, how);
}

bool vomsdata::RetrieveFromProxy(recurse_type how)
{
  gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;

  OM_uint32 major, minor, status;

  major = minor = status = 0;

  major = globus_gss_assist_acquire_cred(&minor, GSS_C_BOTH, &cred);
  if (major != GSS_S_COMPLETE) {
    seterror(VERR_NOIDENT, "Could not load proxy.");
  }
  
  bool b = RetrieveFromCred(cred, how);
  gss_release_cred(&status, &cred);
  return b;
}

bool vomsdata::Retrieve(X509_EXTENSION *ext)
{
  verify_type v = ver_type;
  ver_type = (verify_type)((int)ver_type & (~VERIFY_ID));

  bool ret = evaluate((AC_SEQ*)X509V3_EXT_d2i(ext), "", "", NULL);

  ver_type = v;

  return ret;
}

bool vomsdata::Retrieve(X509 *cert, STACK_OF(X509) *chain, recurse_type how)
{
  bool ok = false;

  std::string subject;
  std::string ca;
  AC_SEQ *acs = NULL;
  X509 *holder = NULL;

  if (retrieve(cert, chain, how, &acs, subject, ca, &holder)) {
    ok = evaluate(acs, subject, ca, holder);
    if (acs)
      AC_SEQ_free(acs);
    if (holder)
      X509_free(holder);
  }

  return ok;
}

bool vomsdata::Import(std::string buffer)
{
  bool result = false;

  X509 *holder;
  char *buf = NULL;

  std::string subject, ca;
  unsigned char *buftmp, *copy;

  char *str;
  int len;

  str = Decode(buffer.c_str(), buffer.size(), &len);
  if (str) {
    buffer = std::string(str, len);
    free(str);
  }
  else {
    seterror(VERR_FORMAT, "Malformed input data.");
    return false;
  }

  do {
    copy = buftmp = (unsigned char *)(const_cast<char *>(buffer.data()));

    holder = d2i_X509(NULL, &copy, buffer.size());

    if (holder) {
      buf = X509_NAME_oneline(X509_get_subject_name(holder), NULL, 0);
      if (buf) 
        subject = std::string(buf);
      OPENSSL_free(buf);
      buf = X509_NAME_oneline(X509_get_issuer_name(holder), NULL, 0);
      if (buf)
        ca = std::string(buf);
      OPENSSL_free(buf);

      voms v;

      buffer = buffer.substr(copy - buftmp);
      result = verifydata(buffer, subject, ca, holder, v);
      if (result)
        data.push_back(v);
      X509_free(holder);
    }
    else {
      seterror(VERR_NOIDENT, "Cannot discovere AC issuer.");
      return false;
    }
  } while (!buffer.empty() &&  result);

  return result;
}

bool vomsdata::Export(std::string &buffer)
{
  std::string result;
  std::string temp;

  if (data.empty()) {
    buffer= "";
    return true;
  }

  for (std::vector<voms>::iterator v=data.begin(); v != data.end(); v++) {
    /* Dump owner's certificate */
    int l;
    unsigned char *xtmp, *xtmp2;

    l = i2d_X509(v->holder, NULL);
    if (!l) {
      seterror(VERR_FORMAT, "Malformed input data.");
      return false;
    }
    if ((xtmp2 = (xtmp = (unsigned char *)OPENSSL_malloc(l)))) {
      i2d_X509(v->holder, &xtmp);
      result += std::string((char *)xtmp2, l);
      OPENSSL_free(xtmp2);
    }
    else {
      seterror(VERR_MEM, "Out of memory!");
      return false;
    }

    /* This is an AC format. */
    int len  = i2d_AC(v->ac, NULL);
    unsigned char *tmp, *tmp2;

    if ((tmp2 = (tmp = (unsigned char *)OPENSSL_malloc(len)))) {
      i2d_AC(v->ac,&tmp);
      result += std::string((char *)tmp2, len);
      OPENSSL_free(tmp2);
    }
    else {
      seterror(VERR_MEM, "Out of memory!");
      return false;
    }
  }

  char *str;
  int len;
  str = Encode(result.c_str(), result.size(), &len);
  if (str) {
    buffer = std::string(str, len);
    free(str);
    return true;
  }
  else
    return false;
}

bool vomsdata::DefaultData(voms &d)
{
  if (data.empty()) {
    seterror(VERR_NOEXT, "No VOMS extensions have been processed.");
    return false;
  }

  d = data.front();
  return true;
}

bool vomsdata::loadfile(std::string filename, uid_t uid, gid_t gid)
{
  struct stat stats;

  struct vomsdata data;

  std::string temp;

  if (filename.empty()) {
    seterror(VERR_DIR, "Filename for vomses file or dir (system or user) unspecified!");
    return false;
  }

  if (stat(filename.c_str(), &stats) == -1) {
    seterror(VERR_DIR, "Cannot find file or dir: " + filename);
    return false;
  }

  if ((stats.st_uid != 0 && stats.st_uid != getuid()) || 
      (stats.st_gid != 0 && stats.st_gid != getgid())) {
    seterror(VERR_DIR, "Wrong ownership on file: " + filename + "\n" +
             "Expected: either (0,0) or (UID, GID) = (" + stringify(uid, temp) +
             ", " + stringify(gid, temp) + ")\n");
    return false;
  }

  if (!(stats.st_mode == (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) ||
        stats.st_mode == (S_IFDIR |
                          S_IRUSR | S_IWUSR | S_IXUSR |
                          S_IRGRP | S_IXGRP |
                          S_IROTH | S_IXOTH))) {
    seterror(VERR_DIR, std::string("Wrong permissions on file: ") + filename + 
             "\nThey should be: 644 (for files) or 755 (for dirs)\n");
    return false;
  }

  if (stats.st_mode & S_IFREG)
    return loadfile0(filename, uid, gid);
  else {
    DIR *dp = opendir(filename.c_str());
    struct dirent *de;

    if (dp) {
      bool cumulative = false;
      while ((de = readdir(dp))) {
        char *name = de->d_name;
        if (name && (strcmp(name, ".") != 0) && (strcmp(name, "..") != 0))
          cumulative |= loadfile(filename + "/" + name, uid, gid);
      }
      closedir(dp);
      return cumulative;
    }
  }
  return false;
}

static bool
tokenize(std::string str, unsigned int &start, std::string &value)
{
  if (start != std::string::npos) {
    unsigned int begin = str.find('"',start);
    if (begin != std::string::npos) {
      unsigned int end = str.find('"',begin+1);
      if (end != std::string::npos) {
        value = str.substr(begin+1, end-begin-1);
        start = end+1;
        if (start >= str.size())
          start = std::string::npos;
        return true;
      }
    }
  }
  return false;
}

static bool empty(std::string c)
{
  if (c[0] == '#')
    return true;

  for (unsigned int i = 0; i < c.size(); i++)
    if (!isspace(c[i]))
      return false;
  return true;
}

bool vomsdata::loadfile0(std::string filename, uid_t uid, gid_t gid)
{
  struct contactdata data;

  if (filename.empty()) {
    seterror(VERR_DIR, "Filename unspecified.");
    return false;
  }

  /* Opens the file */
  std::ifstream f(filename.c_str());

  if (!f) {
    seterror(VERR_DIR, "Cannot open file: " + filename);
    return false;
  }

  /* Load the file */
  int linenum = 1;
  bool ok = true;
  bool verok = true;

  while (ok && f) {
    std::string line;

    if (getline(f,line) && !empty(line)) {
      unsigned int start = 0;
      std::string port, version;

      ok &= tokenize(line, start, data.nick);
      ok &= tokenize(line, start, data.host);
      ok &= tokenize(line, start, port);
      ok &= tokenize(line, start, data.contact);
      ok &= tokenize(line, start, data.vo);
      verok &= tokenize(line, start, version);

      if (ok) {
        data.port = atoi(port.c_str());
        if (verok)
          data.version = atoi(version.c_str());
        else
          data.version = -1;
        servers.push_back(data);
      }
      else {
        seterror(VERR_FORMAT, "data format in file: " + filename + " incorrect!\nLine: " + line);
        return false;
      }
    }
//     else {
//       seterror(VERR_FORMAT, "data format in file: " + filename + " incorrect!\nLine: " + line);
//       return false;
//     }
    linenum++;
  }
  return true;
}

bool vomsdata::LoadSystemContacts(std::string dir)
{
  if (dir.empty())
    dir = "/opt/glite/etc/vomses";

  return loadfile(dir, 0, 0);
}

bool vomsdata::LoadUserContacts(std::string dir)
{
  if (dir.empty()) {
    char *name = getenv("VOMS_USERCONF");
    if (name)
      dir = std::string(name);
    else {
      char *home = getenv("HOME");
      if (home)
        dir = std::string(home) + "/.glite/vomses";
      else
        dir = "~/.glite/vomses";
    }
  }

  return loadfile(dir, 0, 0);
}

std::vector<contactdata>
vomsdata::FindByAlias(std::string nick)
{
  std::vector<contactdata>::iterator beg = servers.begin(), end = servers.end();
  std::vector<contactdata> results;

  while (beg != end) {
    if (beg->nick == nick)
      results.push_back(*beg);
    beg++;
  }

  return std::vector<contactdata>(results);
}

std::vector<contactdata> vomsdata::FindByVO(std::string vo)
{
  std::vector<contactdata>::iterator beg = servers.begin(), end = servers.end();
  std::vector<contactdata> results;

  while (beg != end) {
    if (beg->vo == vo)
      results.push_back(*beg);
    beg++;
  }

  return std::vector<contactdata>(results);
}

voms::voms(const voms &orig)
{
  version   = orig.version;
  siglen    = orig.siglen;
  signature = orig.signature;
  user      = orig.user;
  userca    = orig.userca;
  server    = orig.server;
  serverca  = orig.serverca;
  voname    = orig.voname;
  uri       = orig.uri;
  date1     = orig.date1;
  date2     = orig.date2;
  type      = orig.type;
  std       = orig.std;
  custom    = orig.custom;
  fqan      = orig.fqan;
  serial    = orig.serial;
  ac = (AC *)ASN1_dup((int (*)())i2d_AC, (char * (*)())d2i_AC, (char *)orig.ac);
  holder = (X509 *)ASN1_dup((int (*)())i2d_X509,
			    (char * (*)())d2i_X509,
			    (char *)orig.holder);
}

voms::voms(): version(0), siglen(0), ac(NULL), holder(NULL)
{}

voms &voms::operator=(const voms &orig)
{
  if (this == &orig)
    return *this;
 
  version   = orig.version;
  siglen    = orig.siglen;
  signature = orig.signature;
  user      = orig.user;
  userca    = orig.userca;
  server    = orig.server;
  serverca  = orig.serverca;
  voname    = orig.voname;
  uri       = orig.uri;
  date1     = orig.date1;
  date2     = orig.date2;
  type      = orig.type;
  std       = orig.std;
  custom    = orig.custom;
  fqan      = orig.fqan;
  serial    = orig.serial;
  ac = (AC *)ASN1_dup((int (*)())i2d_AC, (char * (*)())d2i_AC, (char *)orig.ac);
  holder = (X509 *)ASN1_dup((int (*)())i2d_X509, (char * (*)())d2i_X509, (char *)orig.holder);
  return *this;
}

voms::~voms()
{
  AC_free(ac);
  X509_free(holder);
}

int getMajorVersionNumber(void) {return 1;}
int getMinorVersionNumber(void) {return 5;}
int getPatchVersionNumber(void) {return 1;}