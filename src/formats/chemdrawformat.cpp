/**********************************************************************
Copyright (C) 1998-2001 by OpenEye Scientific Software, Inc.
Some portions Copyright (c) 2001-2003 by Geoffrey R. Hutchison
Some portions Copyright (C) 2004 by Chris Morley
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/

#include "mol.h"
#include "obconversion.h"

using namespace std;
namespace OpenBabel
{

class ChemDrawFormat : public OBFormat
{
public:
    //Register this format type ID
    ChemDrawFormat()
    {
        OBConversion::RegisterFormat("ct",this);
    }

    virtual const char* Description() //required
    {
        return
            "ChemDraw Connection Table format \n \
            No comments yet\n \
            ";
    };

    virtual const char* SpecificationURL(){return
            "";}; //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
        return READONEONLY | WRITEONEONLY;
    };

    //*** This section identical for most OBMol conversions ***
    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool ReadMolecule(OBBase* pOb, OBConversion* pConv);
    virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

    ////////////////////////////////////////////////////
    /// The "Convert" interface functions
    virtual bool ReadChemObject(OBConversion* pConv)
    {
        OBMol* pmol = new OBMol;
        bool ret=ReadMolecule(pmol,pConv);
        if(ret) //Do transformation and return molecule
            pConv->AddChemObject(pmol->DoTransformations(pConv->GetGeneralOptions()));
        else
            pConv->AddChemObject(NULL);
        return ret;
    };

    virtual bool WriteChemObject(OBConversion* pConv)
    {
        //Retrieve the target OBMol
        OBBase* pOb = pConv->GetChemObject();
        OBMol* pmol = dynamic_cast<OBMol*> (pOb);
        bool ret=false;
        if(pmol)
            ret=WriteMolecule(pmol,pConv);
        delete pOb;
        return ret;
    };
};
//***

//Make an instance of the format class
ChemDrawFormat theChemDrawFormat;

////////////////////////////////////////////////////////////////

bool ChemDrawFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
{
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);
    if(pmol==NULL)
        return false;

    //Define some references so we can use the old parameter names
    ostream &ofs = *pConv->GetOutStream();
    OBMol &mol = *pmol;
    const char *dimension = pConv->GetDimension();

    char buffer[BUFF_SIZE];

    ofs << mol.GetTitle() << endl;
    sprintf(buffer," %d %d",mol.NumAtoms(),mol.NumBonds());
    ofs << buffer << endl;

    OBAtom *atom;
    vector<OBNodeBase*>::iterator i;

    for(atom = mol.BeginAtom(i);atom;atom = mol.NextAtom(i))
    {
        sprintf(buffer," %9.4f %9.4f    0.0000 %-1s",
                atom->x(),
                atom->y(),
                etab.GetSymbol(atom->GetAtomicNum()));
        ofs << buffer << endl;
    }

    OBBond *bond;
    vector<OBEdgeBase*>::iterator j;

    for(bond = mol.BeginBond(j);bond;bond = mol.NextBond(j))
    {
        sprintf(buffer,"%3d%3d%3d%3d",
                bond->GetBeginAtomIdx(),
                bond->GetEndAtomIdx(),
                bond->GetBO(), bond->GetBO());
        ofs << buffer << endl;
    }
    return(true);
}

bool ChemDrawFormat::ReadMolecule(OBBase* pOb, OBConversion* pConv)
{
  OBMol* pmol = dynamic_cast<OBMol*>(pOb);
  if(pmol==NULL)
    return false;
  
  //Define some references so we can use the old parameter names
  istream &ifs = *pConv->GetInStream();
  OBMol &mol = *pmol;
  const char* title = pConv->GetTitle();

  char buffer[BUFF_SIZE];
  unsigned int natoms, nbonds;
  unsigned int bgn, end, order;
  vector<string> vs;
  OBAtom *atom;
  double x, y, z;

  mol.BeginModify();

  ifs.getline(buffer,BUFF_SIZE);
  if (strlen(buffer) == 0)
    mol.SetTitle(buffer);
  else
    mol.SetTitle(title);

  ifs.getline(buffer,BUFF_SIZE);
  sscanf(buffer," %d %d", &natoms, &nbonds);
  
  for (int i = 1; i <= natoms; i ++)
  {
    if (!ifs.getline(buffer,BUFF_SIZE)) return(false);
    tokenize(vs,buffer);
    if (vs.size() != 4) return(false);
    atom = mol.NewAtom();

    x = atof((char*)vs[0].c_str());
    y = atof((char*)vs[1].c_str());
    z = atof((char*)vs[2].c_str());

    atom->SetVector(x,y,z); //set coordinates
    atom->SetAtomicNum(etab.GetAtomicNum(vs[3].c_str()));
  }

  if (nbonds != 0)
    for (int i = 0; i < nbonds; i++)
      {
	if (!ifs.getline(buffer,BUFF_SIZE)) return(false);
	tokenize(vs,buffer);
	if (vs.size() != 4) return(false);
        if (!sscanf(buffer,"%d%d%d%*d",&bgn,&end,&order)) return (false);
	mol.AddBond(bgn,end,order);
      }

  mol.EndModify();
  return(true);
}


} //namespace OpenBabel
