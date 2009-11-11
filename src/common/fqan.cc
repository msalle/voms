/*********************************************************************
 *
 * Authors: Vincenzo Ciaschini - Vincenzo.Ciaschini@cnaf.infn.it 
 *          Valerio Venturi - Valerio.Venturi@cnaf.infn.it 
 *
 * Copyright (c) 2002-2009 INFN-CNAF on behalf of the EU DataGrid
 * and EGEE I, II and III
 * For license conditions see LICENSE file or
 * http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 * Parts of this code may be based upon or even include verbatim pieces,
 * originally written by other people, in which case the original header
 * follows.
 *
 *********************************************************************/

#include "config.h"

extern "C" {
#include <stdlib.h>
}

#include <iostream>
#include "fqan.h"

std::string parse_fqan(const std::vector<std::string>& fqans)
{
  std::string parsed;
  
  for(std::vector<std::string>::const_iterator i = fqans.begin(); i != fqans.end(); ++i)
  {
    parsed += FQANParse(*i);
    if(i != (fqans.end() - 1))
      parsed += ",";
  }

  return parsed;
}

std::string FQANParse(std::string fqan) 
{
  std::string parsed = fqan;

  /* check if fqan is all */

  if(fqan == "all" || fqan == "ALL")
    parsed = "A";
  else {

    /* check for presence of capability selection */

    std::string::size_type cap_pos = fqan.find("/Capability=");
    if(cap_pos!=std::string::npos) {
      //if(!quiet)
      std::cerr << "capability selection not supported" << std::endl;
      exit(1);
    }

    /* check for role selection*/

    std::string::size_type role_pos = fqan.find("/Role=");
    if (role_pos != std::string::npos && role_pos > 0)
      parsed = "B" + fqan.substr(0, role_pos) + ":" + fqan.substr(role_pos+6);
    else if (role_pos==0)
      parsed = "R" + fqan.substr(role_pos+6);
    else if (fqan[0] == '/')
      parsed = "G" + fqan.substr(0);
  }

  return parsed;
}