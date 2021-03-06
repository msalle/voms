/*********************************************************************
 *
 * Authors: Vincenzo Ciaschini - Vincenzo.Ciaschini@cnaf.infn.it 
 *
 * Copyright (c) Members of the EGEE Collaboration. 2004-2010.
 * See http://www.eu-egee.org/partners/ for details on the copyright holders.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Parts of this code may be based upon or even include verbatim pieces,
 * originally written by other people, in which case the original header
 * follows.
 *
 *********************************************************************/
#include "config.h"
#include "replace.h"

#include "voms_api.h"

/* Interface routines from C++ API to C API */

extern "C" {
#include "cinterface.h"
#include <stdlib.h>
#include <time.h>
#include <listfunc.h>
}

#include "realdata.h"
#include "data.h"

#include <cstring>
#include <cstdlib>

void VOMS_Destroy(struct vomsdatar *vd);

int TranslateVOMS(struct vomsdatar *vd, std::vector<voms> &v, UNUSED(int *error))
{
  if (vd->data) {
    /* Delete old store */
    free(vd->data[0]);
    free(vd->data);
  }

  struct vomsr **arr2 = (struct vomsr **)malloc((v.size()+1) * sizeof(struct vomsr *));

  if (arr2) {
    std::vector<voms>::iterator cur = v.begin();
    std::vector<voms>::iterator end = v.end();
    int i = 0;
    while (cur != end) {
      arr2[i] = cur->translate();
      arr2[i]->mydata = i;
      arr2[i]->my2    = (void *)vd;
      i++;
      ++cur;
    }
    arr2[i] = NULL;
    
    vd->data = arr2;
    return 1;
  }
  free(arr2);
  return 0;
}

static char *
mystrdup(const char *str, int len = 0)
{
  if (!str)
    return NULL;
  else {
    if (!len)
      len = strlen(str);
    char *res = (char*)malloc(len+1);
    if (!res)
      throw std::bad_alloc();
    memcpy(res, str, len);
    res[len]='\0';
    return res;
  }
}

extern "C" {


struct vomsdatar *VOMS_Init(char *voms, char *cert)
{
  struct vomsdatar *vd = NULL;
  try {
    if ((vd = (struct vomsdatar *)malloc(sizeof(struct vomsdatar)))) {
      vd->cdir = mystrdup(voms ? voms : "");
      vd->vdir = mystrdup(cert ? cert : "");
      vd->data = NULL;
      vd->extra_data = vd->workvo = NULL;
      vd->volen = vd->extralen = 0;
      vd->real = new vomsdata((voms ? std::string(voms) : ""),
                              (cert ? std::string(cert) : ""));;
      vd->timeout = -1;
    }
  }
  catch(...) {
    goto err;
  }
  
  return vd;

 err:
  VOMS_Destroy(vd);
  return NULL;
}

#define GetPointer(v) (((struct realdata *)(((struct vomsdatar *)((v)->my2))->real->data[v->mydata].realdata)))
#define GetV(v) (((struct vomsdatar *)((v)->my2))->real->data[v->mydata])

int VOMS_GetAttributeSourcesNumber(struct vomsr *v, UNUSED(struct vomsdatar *vd), int *error)
{
  try {
    return GetV(v).GetAttributes().size();
  }
  catch(...) {
    *error = VERR_PARAM;
    return -1;
  }
}

int VOMS_GetAttributeSourceHandle(struct vomsr *v, int num, struct vomsdatar *vd, int *error)
{
  try {
    if (VOMS_GetAttributeSourcesNumber(v, vd, error) >= num)
      return num;
  }
  catch(...) {
  }
  *error = VERR_PARAM;
  return -1;
}

const char *VOMS_GetAttributeGrantor(struct vomsr *v, int handle, UNUSED(struct vomsdatar *vd), int *error)
{
  try {
    return ((GetV(v).GetAttributes())[handle].grantor.c_str());
  }
  catch(...) {
    *error = VERR_PARAM;
    return NULL;
  }
}

int VOMS_GetAttributesNumber(struct vomsr *v, int handle, UNUSED(struct vomsdatar *vd), int *error)
{
  try {
    return ((GetV(v).GetAttributes())[handle].attributes.size());
  }
  catch (...) {
    *error = VERR_PARAM;
    return -1;
  }
}

int VOMS_GetAttribute(struct vomsr *v, int handle, int num, struct attributer *at, UNUSED(struct vomsdatar *vd), int *error)
{
  try {
    struct attribute a = ((GetV(v).GetAttributes())[handle]).attributes[num];

    at->name = a.name.c_str();
    at->qualifier = (a.qualifier.empty() ? NULL : a.qualifier.c_str());
    at->value = a.value.c_str();
    return 1;
  }
  catch(...) {
    *error = VERR_PARAM;
    return 0;
  }
}

static struct contactdatar **Arrayize(std::vector<contactdata> &cd, int *error)
{

  if (cd.empty())
    return NULL;

  int size1 = cd.size() * sizeof(struct contactdatar);
  int size2 = (cd.size()+1) * sizeof(struct contactdatar *);

  struct contactdatar **cdr = (struct contactdatar **)malloc(size2);
  struct contactdatar *cda = (struct contactdatar *)malloc(size1);

  if (cdr && cda) {
    std::vector<contactdata>::const_iterator cur = cd.begin(),
      end = cd.end();

    int i = 0;
    while (cur != end) {
      cdr[i] = &cda[i];

      cda[i].nick    = mystrdup(cur->nick.c_str());
      cda[i].host    = mystrdup(cur->host.c_str());
      cda[i].contact = mystrdup(cur->contact.c_str());
      cda[i].vo      = mystrdup(cur->vo.c_str());
      cda[i].port    = cur->port;
      cda[i].version = cur->version;
      i++;
      ++cur;
    }
    cdr[i] = NULL;
    
    return cdr;
  }
  else {
    *error = VERR_MEM;
    free(cdr);
    free(cda);
    return NULL;
  }
}

struct contactdatar **VOMS_FindByVO(struct vomsdatar *vd, char *vo,
                                    char *system, char *user, int *error)
{
  if (!vd || !vd->real || !vo || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  vomsdata *v = (vomsdata *)vd->real;

  (void)v->LoadSystemContacts(system ? std::string(system) : "");
  (void)v->LoadUserContacts(user ? std::string(user) : "");

  std::vector<contactdata> cd = v->FindByVO(vo);

  if (!cd.empty())
    return Arrayize(cd, error);

  *error = v->error;
  return NULL;
}

struct contactdatar **VOMS_FindByAlias(struct vomsdatar *vd, char *vo,
                                       char *system, char *user, int *error)
{
  if (!vd || !vd->real || !vo || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  vomsdata *v = (vomsdata *)vd->real;

  (void)v->LoadSystemContacts(system ? std::string(system) : "");
  (void)v->LoadUserContacts(user ? std::string(user) : "");

  std::vector<contactdata> cd = v->FindByAlias(vo);

  if (!cd.empty())
    return Arrayize(cd, error);

  *error = v->error;
  return NULL;
}

void VOMS_DeleteContacts(struct contactdatar **list)
{
  if (list) {
    free(list[0]);
    free(list);
  }
}


struct vomsr *voms::translate()
{
  struct vomsr *dst = NULL;

  if ((dst = (struct vomsr *)calloc(1, sizeof(struct vomsr)))) {
    try {
      dst->version   = version;
      dst->siglen    = siglen;
      dst->signature = mystrdup(signature.c_str(), signature.size());
      dst->user      = mystrdup(user.c_str());
      dst->userca    = mystrdup(userca.c_str());
      dst->server    = mystrdup(server.c_str());
      dst->serverca  = mystrdup(serverca.c_str());
      dst->voname    = mystrdup(voname.c_str());
      dst->uri       = mystrdup(uri.c_str());
      dst->date1     = mystrdup(date1.c_str());
      dst->date2     = mystrdup(date2.c_str());
      dst->type      = type;
      dst->custom    = mystrdup(custom.c_str(), custom.size());
      dst->serial    = mystrdup(serial.c_str());
      dst->datalen   = custom.size();

      dst->ac     = AC_dup((((struct realdata *)realdata)->ac));
      dst->holder = X509_dup(holder);

      if ((!dst->holder && holder) || !dst->ac)
        throw 3;

      dst->fqan = vectoarray(fqan);
      if (!dst->fqan)
        throw 3;

      dst->std  = (struct datar **)calloc(1, sizeof(struct datar *)*(std.size()+1));
      if (!dst->std)
        throw 3;

      int j = 0;

      std::vector<data>::const_iterator end = std.end();
      for (std::vector<data>::const_iterator i = std.begin();
             i != end; ++i) {
        struct datar *d = (struct datar *)calloc(1, sizeof(struct datar));
        if (d) {
          dst->std[j++] = d;
          d->group = mystrdup(i->group.c_str());
          d->role  = mystrdup(i->role.c_str());
          d->cap   = mystrdup(i->cap.c_str());
        }
        else
          throw 3;
      }

      return dst;
    }
    catch (...) {
      VOMS_Delete(dst);
      return NULL;
    }
  }
  return NULL;
}


static void freeDatar(struct datar *dr)
{
  if (dr){
    free(dr->group);
    free(dr->role);
    free(dr->cap);
    free(dr);
  }
}

void VOMS_Delete(struct vomsr *v) 
{
  if (v) {
    listfree(v->fqan, free);
    listfree((char**)v->std, (freefn)freeDatar);
    free(v->signature);
    free(v->user);
    free(v->userca);
    free(v->server);
    free(v->serverca);
    free(v->voname);
    free(v->uri);
    free(v->date1);
    free(v->date2);
    free(v->custom);
    free(v->serial);
    AC_free(v->ac);
    X509_free(v->holder);
  }

  free(v);
}

struct vomsdatar *VOMS_CopyALL(struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  *error = VERR_MEM;

  return VOMS_Duplicate(vd);
}

void VOMS_Destroy(struct vomsdatar *vd)
{
  if (vd) {
    free(vd->cdir);
    free(vd->vdir);
    free(vd->workvo);
    free(vd->extra_data);
    listfree((char**)vd->data, (freefn)VOMS_Delete);
    delete vd->real;
    free(vd);
  }
}

int VOMS_LoadCredentials(X509 *cert, EVP_PKEY *pkey, STACK_OF(X509) *chain, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  return v->LoadCredentials(cert, pkey, chain) ? 1 : 0;
}

int VOMS_AddTarget(struct vomsdatar *vd, char *target, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  if (target)
    v->AddTarget(std::string(target));

  return 1;
}

void VOMS_FreeTargets(struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return;
  }

  vomsdata *v = vd->real;

  v->ResetTargets();
}

char *VOMS_ListTargets(struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  vomsdata *v = vd->real;

  std::vector<std::string> list = v->ListTargets();

  std::vector<std::string>::const_iterator cur = list.begin();
  std::vector<std::string>::const_iterator begin = list.begin();
  std::vector<std::string>::const_iterator end = list.end();
  std::string total = "";

  while(cur != end) {
    if (cur != begin)
      total += ",";
    total += *cur;
    ++cur;
  }

  char *res = mystrdup(total.c_str());
  if (!res)
    *error = VERR_MEM;
  return res;
}

int VOMS_SetVerificationType(int type, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  v->SetVerificationType(verify_type(type));

  return 1;
}

int VOMS_SetVerificationTime(time_t vertime, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  v->SetVerificationTime(vertime);

  return 1;
}

int VOMS_SetLifetime(int length, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;
  v->SetLifetime(length);
  return 1;
}

int VOMS_SetTimeout(int t, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vd->timeout = t;
  return 1;
}

int VOMS_Ordering(char *order, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  if (order) {
    vomsdata *v = vd->real;
    v->Order(std::string(order));
  }

  return 1;
}

int VOMS_ResetOrder(struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;
  v->ResetOrder();
  return 1;
}


int VOMS_Contact(char *host, int port, char *servsub, char *comm, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;
  if (v->Contact(std::string(host), port, std::string(servsub), std::string(comm), vd->timeout)) {
    return TranslateVOMS(vd, v->data, error);
  }

  *error = v->error;
  return 0;
}

int VOMS_ContactRaw(char *host, int port, char *servsub, char *comm, void **data,
                    int *datalen, int *version, struct vomsdatar *vd, int *error)
{
  if (!host || !port || !servsub || !comm || !data || !datalen || !version ||
      !vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  std::string output;
  
  if (v->ContactRaw(std::string(host), port, std::string(servsub),
                    std::string(comm), output, *version, vd->timeout)) {
    *datalen = output.size();
    char *d = (char *)malloc(output.size());
    if (d) {
      memcpy(d, output.data(), *datalen);
      *data = d;
      return 1;
    }
    else {
      *error = VERR_MEM;
      return 0;
    }
  }

  *error = v->error;
  return 0;
}

int VOMS_Retrieve(X509 *cert, STACK_OF(X509) *chain, int how,
                  struct vomsdatar *vd, int *error)
{
  if (!cert || !vd || !vd->real || !error || (!chain && how == RECURSE_CHAIN)) {
    *error = VERR_PARAM;
    return 0;
  }
  
  vomsdata *v = vd->real;

  if (v->Retrieve(cert, chain, recurse_type(how)))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_RetrieveEXT(X509_EXTENSION *ext, struct vomsdatar *vd, int *error)
{
  if (!ext || !vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }
  
  vomsdata *v = vd->real;

  if (v->Retrieve(ext))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_RetrieveFromFile(FILE *file, int how, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }
  
  vomsdata *v = vd->real;

  if (v->Retrieve(file, recurse_type(how)))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_RetrieveFromCred(gss_cred_id_t cred, int how, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }
  
  vomsdata *v = vd->real;

  if (v->RetrieveFromCred(cred, recurse_type(how)))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_RetrieveFromCtx(gss_ctx_id_t ctx, int how, struct vomsdatar *vd, int *error)
{
  if (error)
    *error = VERR_NOTAVAIL;

  return 0;
}

int VOMS_RetrieveFromProxy(int how, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  if (v->RetrieveFromProxy(recurse_type(how)))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_RetrieveFromAC(AC *ac, struct vomsdatar *vd, int *error)
{
  if (!vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  if (v->Retrieve(ac))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_Import(char *buffer, int buflen, struct vomsdatar *vd, int *error)
{
  if (!buffer || !buflen || !vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  if (v->Import(std::string(buffer, buflen)))
    return TranslateVOMS(vd, v->data, error);

  *error = v->error;
  return 0;
}

int VOMS_Export(char **buffer, int *buflen, struct vomsdatar *vd, int *error)
{

  if (!buffer || !buflen || !vd || !vd->real || !error) {
    *error = VERR_PARAM;
    return 0;
  }

  vomsdata *v = vd->real;

  std::string data;
  if (v->Export(data)) {
    *buflen = data.size();

    char *d;
    if ((d = (char *)malloc(*buflen))) {
      memcpy(d, data.data(), *buflen);
      *buffer = d;
      return 1;
    }
    else {
      *error = VERR_MEM;
      return 0;
    }
  }

  *error = v->error;
  return 0;
}

struct vomsr *VOMS_DefaultData(struct vomsdatar *vd, int *error)
{
  if (!vd || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  return vd->data[0];
}

struct vomsr *VOMS_Copy(struct vomsr *org, int *error)
{
  if (!org || !error) {
    *error = VERR_PARAM;
    return NULL;
  }

  *error = VERR_MEM;

  struct vomsr *dst = NULL;


  if ((dst = (struct vomsr *)calloc(1, sizeof(struct vomsr)))) {
    try {
      dst->version   = org->version;
      dst->siglen    = org->siglen;
      dst->signature = mystrdup(org->signature, org->siglen);
      dst->user      = mystrdup(org->user);
      dst->userca    = mystrdup(org->userca);
      dst->server    = mystrdup(org->server);
      dst->serverca  = mystrdup(org->serverca);
      dst->voname    = mystrdup(org->voname);
      dst->uri       = mystrdup(org->uri);
      dst->date1     = mystrdup(org->date1);
      dst->date2     = mystrdup(org->date2);
      dst->type      = org->type;
      dst->custom    = mystrdup(org->custom, org->datalen);
      dst->serial    = mystrdup(org->serial);
      dst->datalen   = org->datalen;

      dst->ac        = AC_dup(org->ac);
      dst->holder    = X509_dup(org->holder);
      dst->mydata    = org->mydata;
      dst->my2       = org->my2;

      if (!dst->holder || !dst->ac)
        throw 3;

      int size = 0;
      while (org->fqan[size++])
        ;

      dst->fqan = (char **)calloc(1, sizeof(char *)*size);

      size = 0;
      while (org->std[size++])
        ;

      dst->std  = (struct datar **)calloc(1, sizeof(struct datar *)*size);
      if (!(dst->fqan) || !(dst->std))
        throw 3;

      int j = 0;

      while(org->fqan[j]) {
        if (!(dst->fqan[j] = mystrdup(org->fqan[j])))
          throw 3;
        j++;
      }

      j = 0;

      while (org->std[j]) {
        struct datar *d = (struct datar *)calloc(1, sizeof(struct datar));
        if (d) {
          dst->std[j] = d;
          d->group = mystrdup(org->std[j]->group);
          d->role  = mystrdup(org->std[j]->role);
          d->cap   = mystrdup(org->std[j++]->cap);
        }

        if (!d || !d->group || !d->role || !d->cap)
          throw 3;
      }

      return dst;
    }
    catch (...) {
      VOMS_Delete(dst);
      return NULL;
    }
  }
  return NULL;
}

char *VOMS_ErrorMessage(struct vomsdatar *vd, int error, char *buffer, int len)
{
  if (!vd || !vd->real || (buffer && !len)) {
    return NULL;
  }

  vomsdata *v = vd->real;

  std::string msg;

  switch (error) {
  case VERR_MEM:
    msg = "Out of memory.";
    break;
  case VERR_PARAM:
    msg = "Parameters incorrect.";
    break;
  default:
    msg = v->ErrorMessage();
    break;
  }

  if (buffer) {
    if ((msg.size()+1) <= (unsigned int)len) {
      strcpy(buffer, msg.c_str());
      return buffer;
    }
    else
      return NULL;
  }
  else {
    char *buf = (char*)malloc(msg.size()+1);
    if (buf)
      strcpy(buf, msg.c_str());
    return buf;
  }
}
vomsdatar *VOMS_Duplicate(vomsdatar *orig)
{
  struct vomsdatar *vd = NULL;

  try {
    if ((vd = (struct vomsdatar *)malloc(sizeof(struct vomsdatar)))) {
      int error = 0;

      vd->cdir = (orig->cdir ? strdup(orig->cdir) : NULL );
      vd->vdir = (orig->vdir ? strdup(orig->vdir) : NULL );
      vd->data = NULL;
      vd->extra_data = (orig->extra_data ? strdup(orig->extra_data) : NULL);
      vd->workvo = (orig->workvo ? strdup(orig->workvo) : NULL);
      vd->volen = orig->volen;
      vd->extralen = orig->extralen;
      vd->timeout = orig->timeout;
      vd->real = new vomsdata(*(orig->real));

      if (!TranslateVOMS(vd, vd->real->data, &error))
        goto err;
    }
  }
  catch(...) {
    goto err;
  }

  return vd;

 err:
  VOMS_Destroy(vd);
  return NULL;
}

AC *VOMS_GetAC(vomsr *v)
{
  return AC_dup(v->ac);
}

char **VOMS_GetTargetsList(struct vomsr *v, struct vomsdatar *vd, int *error)
{
  if (!v || !vd) {
    if (error)
      *error = VERR_PARAM;
    return NULL;
  }

  std::vector<std::string> targets = GetV(v).GetTargets();

  return vectoarray(targets);
}


void VOMS_FreeTargetsList(char **targets)
{
  listfree(targets, free);
}

}
