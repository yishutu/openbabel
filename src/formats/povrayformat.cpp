/**********************************************************************
Copyright (C) 2002 by Steffen Reith <streit@streit.cc>
Some portions Copyright (c) 2003 by Geoffrey R. Hutchison
Some portions Copyright (C) 2004 by Chris Morley
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/

/* ---- C includes ---- */
#include <math.h>
#include <time.h>
#include <stdlib.h>

/* ---- C++ includes ---- */
#include "babelconfig.h"
#include <string>
#if defined(HAVE_SSTREAM)
#include <sstream>
#else
#include <strstream>
#endif

/* ---- OpenBabel include ---- */
#include "mol.h"
#include "obconversion.h"

/* ---- Max. length of a atom-label ---- */
#define StrLen 32

/* ---- Define max. length of domainname ---- */
#define MAXDOMAINNAMELEN 256

/* ---- Maximal radius of an atom. Needed for bounding box ---- */
#define MAXRADIUS (double) 3.0

/* ---- Define index of first atom if needed ---- */
#ifndef MIN_ATOM
#define MIN_ATOM 1
#endif

/* ---- Size of time-string ---- */
#define TIME_STR_SIZE 64

/* ---- if x < = EPSILON then x = 0.0 ---- */
#define EPSILON (double) 1e-4

/* ---- Define makro for calculating x^2 ---- */
#ifdef SQUARE
#undef SQUARE
#endif
#define SQUARE(x) ((x) * (x))

/* ---- Define PI (if needed) ---- */
#ifndef PI
#define PI ((double) 3.1415926535897932384626433)
#endif

/* ---- Convert RAD to DEG ---- */
#define RAD2DEG(r) (((double) 180.0 * r) / PI)

using namespace std;
namespace OpenBabel
{

class PovrayFormat : public OBFormat
{
public:
    //Register this format type ID
    PovrayFormat()
    {
        OBConversion::RegisterFormat("POVRAY",this);
    }

    virtual const char* Description() //required
    {
        return
            "Povray format\n \
            No comments yet\n \
            ";
    };

    virtual const char* SpecificationURL(){return
            "";}; //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
        return NOTREADABLE;
    };

    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

    ////////////////////////////////////////////////////
    /// The "Convert" interface functions
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

//Make an instance of the format class
PovrayFormat thePovrayFormat;

/* ---- Make a prefix from title of molecule ---- */
string MakePrefix(const char* title)
{
    int pos; /* Position in prefix */
    char *titleCpy = (char*) NULL;
    char *str = (char*) NULL;
    char *last = (char*) NULL;

    /* ---- Copy molecule title to 'str' ---- */
    if ((titleCpy = strdup(title)) == (char *) NULL)
        return string("NoMemory");

    /* --- Find last '/' and set 'str' to it if needed ----- */
    last = strrchr(titleCpy, '/');
    if (last != (char *) NULL)
        str = (last + 1);
    else
        str = titleCpy;

    /* ---- Check for nonempty string ---- */
    if (strlen(str) == 0)
        return string("InValid");

    /* ---- Look for first . and replace with \0 ----- */
    pos = 0;
    while((str[pos] != '\0') && (str[pos] != '.'))
    {

        /* ---- Remove all tabs and spaces ---- */
        if ((str[pos] == ' ') || (str[pos] == '\t'))
            str[pos] = '_';

        /* ---- Check next position ---- */
        pos++;

    }

    /* ---- If we have found a '.' cut the string there ---- */
    str[pos] = '\0';

    /* ---- Cast to C++ string-type the above operations are a mess with C++ strings ---- */
    string prefix(str);

    /* ---- Free allocated memory ---- */
    free(titleCpy);

    /* ---- Return the prefix ---- */
    return prefix;

}

void OutputHeader(ostream &ofs, OBMol &mol, string prefix)
{
    time_t akttime;                              /* Systemtime                        */
    char timestr[TIME_STR_SIZE + 1] = "";        /* Timestring                        */
    size_t time_res;                             /* Result of strftime                */

    /* ---- Get the system-time ---- */
    akttime = time((time_t *) NULL);
    time_res = strftime(timestr,
                        TIME_STR_SIZE,
                        "%a %b %d %H:%M:%S %Z %Y",
                        localtime((time_t *) &akttime)
                       );

    /* ---- Write some header information ---- */
    ofs << "//Povray V3.1 code generated by Open Babel" << endl;
    ofs << "//Author: Steffen Reith <streit@streit.cc>" << endl;

    /* ---- Include timestamp in header ---- */
    ofs << "//Date: " << timestr << endl << endl;

    /* ---- Include header statement for babel ---- */
    ofs << "//Include header for povray" << endl;
    ofs << "#include \"babel31.inc\"" << endl << endl;

    /* ---- You should do a spacefill model for molecules without bonds ---- */
    if (mol.NumBonds() == 0)
    {

        /* ---- Check if a spacefill-model is selected ---- */
        ofs << "#if (BAS | CST)\"" << endl;
        ofs << "#warning \"Molecule without bonds!\"" << endl;
        ofs << "#warning \"You should do a spacefill-model\"" << endl;
        ofs << "#end" << endl << endl;

    }

    /* ---- Set version ---- */
    ofs << "//Use PovRay3.1" << endl;
    ofs << "#version 3.1;" << endl << endl;

    /* ---- Print of name of molecule (#\b depends on size of babel.inc!) ---- */
    ofs << "//Print name of molecule while rendering" << endl;
    ofs << "#render \"\\b\\b " << mol.GetTitle() << "\\n\\n\"" << endl << endl;

}

void CalcBoundingBox(OBMol &mol,
                     double &min_x, double &max_x,
                     double &min_y, double &max_y,
                     double &min_z, double &max_z
                    )
{
    /* ---- Init bounding-box variables ---- */
    min_x = (double) 0.0;
    max_x = (double) 0.0;
    min_y = (double) 0.0;
    max_y = (double) 0.0;
    min_z = (double) 0.0;
    max_z = (double) 0.0;

    /* ---- Check all atoms ---- */
    for(unsigned int i = 1; i <= mol.NumAtoms(); ++i)
    {

        /* ---- Get a pointer to ith atom ---- */
        OBAtom *atom = mol.GetAtom(i);

        /* ---- Check for minimal/maximal x-position ---- */
        if (atom -> GetX() < min_x)
            min_x = atom -> GetX();
        if (atom -> GetX() > max_x)
            max_x = atom -> GetX();

        /* ---- Check for minimal/maximal y-position ---- */
        if (atom -> GetY() < min_y)
            min_y = atom -> GetY();
        if (atom -> GetY() > max_y)
            max_y = atom -> GetY();

        /* ---- Check for minimal/maximal z-position ---- */
        if (atom -> GetZ() < min_z)
            min_z = atom -> GetZ();
        if (atom -> GetZ() > max_z)
            max_z = atom -> GetZ();

    }

}

void OutputAtoms(ostream &ofs, OBMol &mol, string prefix)
{
    /* ---- Write all coordinates ---- */
    ofs << "//Coodinates of atoms 1 - " << mol.NumAtoms() << endl;
    unsigned int i;
    for(i = 1; i <= mol.NumAtoms(); ++i)
    {

        /* ---- Get a pointer to ith atom ---- */
        OBAtom *atom = mol.GetAtom(i);

        /* ---- Write position of atom i ---- */
        ofs << "#declare " << prefix << "_pos_" << i << " = <"
        << atom -> GetX() << ","
        << atom -> GetY() << ","
        << atom -> GetZ()
        << ">;" << endl;

    }

    /* ---- Write povray-description of all atoms ---- */
    ofs << endl << "//Povray-description of atoms 1 - " << mol.NumAtoms() << endl;
    for(i = 1; i <= mol.NumAtoms(); ++i)
    {

        /* ---- Get a pointer to ith atom ---- */
        OBAtom *atom = mol.GetAtom(i);

        /* ---- Write full description of atom i ---- */
        ofs << "#declare " << prefix << "_atom" << i << " = ";
        ofs << "object {" << endl
        << "\t  Atom_" << etab.GetSymbol(atom->GetAtomicNum()) << endl
        << "\t  translate " << prefix << "_pos_" << i << endl << "\t }" << endl;

    }

    /* ---- Add empty line ---- */
    ofs << endl;

}


void OutputBASBonds(ostream &ofs, OBMol &mol, string prefix)
{
    /* ---- Write povray-description of all bonds---- */
    for(unsigned int i = 0; i < mol.NumBonds(); ++i)
    {

        double x1,y1,z1,x2,y2,z2; /* Start and stop coordinates of a bond       */
        double dist;              /* Distance between (x1|y1|z1) and (x2|y2|z2) */
        double phi,theta;         /* Angles between (x1|y1|z1) and (x2|y2|z2)   */
        double dy;                /* Distance between (x1|0|z1) and (x2|0|z2)   */

        /* ---- Get a pointer to ith atom ---- */
        OBBond *bond = mol.GetBond(i);

        /* ---- Assign start of bond i ---- */
        x1 = (bond -> GetBeginAtom()) -> GetX();
        y1 = (bond -> GetBeginAtom()) -> GetY();
        z1 = (bond -> GetBeginAtom()) -> GetZ();

        /* ---- Assign end of bond i ---- */
        x2 = (bond -> GetEndAtom()) -> GetX();
        y2 = (bond -> GetEndAtom()) -> GetY();
        z2 = (bond -> GetEndAtom()) -> GetZ();

        /* ---- Calculate length of bond and (x1|0|z1) - (x2|0|z2) ---- */
        dist = sqrt(SQUARE(x2-x1) + SQUARE(y2-y1) + SQUARE(z2-z1));
        dy = sqrt(SQUARE(x2-x1) + SQUARE(z2-z1));

        /* ---- Calculate Phi and Theta ---- */
        phi = (double) 0.0;
        theta = (double) 0.0;
        if (fabs(dist) >= EPSILON)
            phi = acos((y2-y1)/dist);
        if (fabs(dy) >= EPSILON)
            theta = acos((x2-x1)/dy);

        /* ---- Full description of bond i ---- */
        ofs << "#declare " << prefix << "_bond" << i
        << " = object {" << endl << "\t  bond_" << bond -> GetBondOrder() << endl;

        /* ---- Scale bond if needed ---- */
        if (fabs(dist) >= EPSILON)
        {

            /* ---- Print povray scale-statement (x-Axis) ---- */
            ofs << "\t  scale <" << dist << ",1.0000,1.0000>\n";

        }

        /* ---- Rotate (Phi) bond if needed ---- */
        if (fabs(RAD2DEG(-phi) + (double) 90.0) >= EPSILON)
        {

            /* ---- Rotate along z-axis ---- */
            ofs << "\t  rotate <0.0000,0.0000,"
            << RAD2DEG(-phi) + (double) 90.0
            << ">" << endl;


        }

        /* ---- Check angle between (x1|0|z1) and (x2|0|z2) ---- */
        if (theta >= EPSILON)
        {

            /* ---- Check direction ---- */
            if ((z2 - z1) >= (double) 0.0)
            {

                /* ---- Rotate along y-Axis (negative) ---- */
                ofs << "\t  rotate <0.0000,"
                << RAD2DEG((double) -1.0 * theta) << ",0.0000>"
                << endl;

            }
            else
            {

                /* ---- Rotate along y-Axis (positive) ---- */
                ofs << "\t  rotate <0.0000,"
                << RAD2DEG(theta) << ",0.0000>"
                << endl;

            }

        }

        /* ---- Translate bond to start ---- */
        ofs << "\t  translate " << prefix << "_pos_" << bond -> GetBeginAtomIdx()
        << endl << "\t }" << endl;

    }

}

void OutputCSTBonds(ostream &ofs, OBMol &mol, string prefix)
{
    /* ---- Write povray-description of all bonds---- */
    for(unsigned int i = 0; i < mol.NumBonds(); ++i)
    {

        double x1,y1,z1,x2,y2,z2; /* Start and stop coordinates of a bond       */
        double dist;              /* Distance between (x1|y1|z1) and (x2|y2|z2) */
        double phi,theta;         /* Angles between (x1|y1|z1) and (x2|y2|z2)   */
        double dy;                /* Distance between (x1|0|z1) and (x2|0|z2)   */

        /* ---- Get a pointer to ith atom ---- */
        OBBond *bond = mol.GetBond(i);

        /* ---- Assign start of bond i ---- */
        x1 = (bond -> GetBeginAtom()) -> GetX();
        y1 = (bond -> GetBeginAtom()) -> GetY();
        z1 = (bond -> GetBeginAtom()) -> GetZ();

        /* ---- Assign end of bond i ---- */
        x2 = (bond -> GetEndAtom()) -> GetX();
        y2 = (bond -> GetEndAtom()) -> GetY();
        z2 = (bond -> GetEndAtom()) -> GetZ();

        /* ---- Calculate length of bond and (x1|0|z1) - (x2|0|z2) ---- */
        dist = sqrt(SQUARE(x2-x1) + SQUARE(y2-y1) + SQUARE(z2-z1));
        dy = sqrt(SQUARE(x2-x1) + SQUARE(z2-z1));

        /* ---- Calculate Phi and Theta ---- */
        phi = (double) 0.0;
        theta = (double) 0.0;
        if (fabs(dist) >= EPSILON)
            phi = acos((y2-y1)/dist);
        if (fabs(dy) >= EPSILON)
            theta = acos((x2-x1)/dy);

        /* ---- Begin of description of bond i (for a capped sticks model) ---- */
        ofs << "#declare " << prefix << "_bond" << i << " = object {" << endl;
        ofs << "\t  union {" << endl;

        /* ---- Begin of Start-Half of Bond (i) ---- */
        ofs << "\t   object {" << endl << "\t    bond_" << bond -> GetBondOrder()  << "\n";

        /* ---- Add a pigment - statement for start-atom of bond ---- */
        ofs << "\t    pigment{color Color_"
        << bond -> GetBeginAtom() -> GetType()
        << "}" << endl;

        /* ---- Scale bond if needed ---- */
        if (fabs((double) 2.0 * dist) >= EPSILON)
        {

            /* ---- Print povray scale-statement (x-Axis) ---- */
            ofs << "\t    scale <" << (double) 0.5 * dist << ",1.0000,1.0000>" << endl;

        }

        /* ---- Rotate (Phi) bond if needed ---- */
        if (fabs(RAD2DEG(-phi) + (double) 90.0) >= EPSILON)
        {

            /* ---- Rotate along z-axis ---- */
            ofs << "\t    rotate <0.0000,0.0000,"
            << RAD2DEG(-phi) + (double) 90.0
            << ">" << endl;

        }

        /* ---- Check angle between (x1|0|z1) and (x2|0|z2) ---- */
        if (theta >= EPSILON)
        {

            /* ---- Check direction ---- */
            if ((z2 - z1) >= (double) 0.0)
            {

                /* ---- Rotate along y-Axis (negative) ---- */
                ofs << "\t    rotate <0.0000,"
                << RAD2DEG((double) -1.0 *theta) << ",0.0000>"
                << endl;

            }
            else
            {

                /* ---- Rotate along y-Axis (positive) ---- */
                ofs << "\t    rotate <0.0000," << RAD2DEG(theta) << ",0.0000>" << endl;

            }

        }

        /* ---- Translate bond to start ---- */

        ofs << "\t    translate " << prefix << "_pos_" << bond -> GetBeginAtomIdx() << endl;

        /* ---- End of description of Start-Bond ---- */
        ofs << "\t   }" << endl;

        /* ---- Begin of End-Half of Bond i ---- */
        ofs << "\t   object {" << endl << "\t    bond_" << bond -> GetBondOrder() << endl;

        /* ---- Add a pigment - statement for end-atom of bond i ---- */
        ofs << "\t    pigment{color Color_"
        << bond -> GetEndAtom() -> GetType()
        << "}" << endl;

        /* ---- Scale bond if needed ---- */
        if (fabs((double) 2.0 * dist) >= EPSILON)
        {

            /* ---- Print povray scale-statement (x-Axis) ---- */
            ofs << "\t    scale <" << (double) 0.5 * dist << ",1.0000,1.0000>" << endl;

        }

        /* ---- Rotate (Phi) bond if needed ---- */
        if (fabs(RAD2DEG(-phi) + (double) 270.0) >= EPSILON)
        {

            /* ---- Rotate along z-axis (oposite to start half) ---- */
            ofs << "\t    rotate <0.0000,0.0000,"
            << (RAD2DEG(-phi) + (double) 90.0) + (double) 180.0
            << ">" << endl;

        }

        /* ---- Check angle between (x1|0|z1) and (x2|0|z2) ---- */
        if (fabs(theta) >= EPSILON)
        {

            /* ---- Check direction ---- */
            if ((z2 - z1) >= (double) 0.0)
            {

                /* ---- Rotate along y-Axis (negative) (oposite orientation) ---- */
                ofs << "\t    rotate <0.0000,"
                << RAD2DEG((double) -1.0 * theta)
                << ",0.0000>"
                << endl;

            }
            else
            {

                /* ---- Rotate along y-Axis (positive) (oposite orientation) ---- */
                ofs << "\t    rotate <0.0000," << RAD2DEG(theta) << ",0.0000>" << endl;

            }

        }

        /* ---- Translate bond to end ---- */
        ofs << "\t    translate " << prefix << "_pos_" << bond -> GetEndAtomIdx() << endl;

        /* ---- End of description of End-Bond ---- */
        ofs << "\t   }" << endl;

        /* ---- End of description of bond i ---- */
        ofs << "\t  }" << endl << "\t }" << endl << endl;

    }

}

void OutputUnions(ostream &ofs, OBMol &mol, string prefix)
{
    /* ---- Build union of all atoms ---- */
    ofs << endl << "//All atoms of molecule " << prefix << endl;
    ofs << "#ifdef (TRANS)" << endl;
    ofs << "#declare " << prefix << "_atoms = merge {" << endl;
    ofs << "#else" << endl;
    ofs << "#declare " << prefix << "_atoms = union {" << endl;
    ofs << "#end //(End of TRANS)" << endl;

    /* ---- Write definition of all atoms ---- */
    for(unsigned int i = 1; i <= mol.NumAtoms(); ++i)
    {

        /* ---- Write definition of atom i ---- */
        ofs << "\t  object{" << prefix << "_atom" << i << "}" << endl;

    }
    ofs << "\t }" << endl << endl;

    /* ---- Check for number of bonds ---- */
    if(mol.NumBonds() > 0)
    {

        /* ---- Do a BAS or CST model ? ---- */
        ofs << "//Bonds only needed for ball and sticks or capped sticks models" << endl;
        ofs << "#if (BAS | CST)" << endl;
        ofs << "#declare " << prefix <<"_bonds = union {" << endl;

        /* ---- Description of all bonds ---- */
        for(unsigned int i = 0; i < mol.NumBonds(); ++i)
        {

            /* ---- Write Definition of Bond i ---- */
            ofs << "\t  object{" << prefix << "_bond" << i << "}" << endl;

        }

        /* ---- End of povray-conditional for ball and sticks ---- */
        ofs << "\t }" << endl << "#end" << endl << endl;

    }

}

void OutputMoleculeBonds(ostream &ofs,
                         string prefix,
                         double min_x, double max_x,
                         double min_y, double max_y,
                         double min_z, double max_z
                        )
{
    /* ---- Write a comment ---- */
    ofs << endl << "//Definition of molecule " << prefix << endl;

    /* ---- Check for space-fill model ---- */
    ofs << "#if (SPF)" << endl;
    ofs << "#declare " << prefix << " = object{"
    << endl << "\t  " << prefix << "_atoms" << endl;

    /* ---- Here we do BAS oder CST models ---- */
    ofs << "#else" << endl;
    ofs << "#declare " << prefix << " = union {" << endl;

    /* ---- Add all Atoms ---- */
    ofs << "\t  object{" << prefix << "_atoms}" << endl;

    /* ---- Add difference between bonds and atoms ---- */
    ofs << "#if (BAS | CST)//(Not really needed at moment!)" << endl;

    /* ---- Use disjunct objects for transparent pics? ---- */
    ofs << "#if (TRANS)" << endl;
    ofs << "\t  difference {" << endl;
    ofs << "\t   object{" << prefix << "_bonds}" << endl
    << "\t   object{" << prefix << "_atoms}" << endl
    << "\t  }" << endl;

    /* ---- Do a solid model ? ---- */
    ofs << "#else" << endl;
    ofs << "\t  object{" << prefix << "_bonds}" << endl;
    ofs << "#end //(End of TRANS)" << endl;
    ofs << "#end //(End of (BAS|CST))" << endl;

    /* ---- End of CST or BAS model ---- */
    ofs << "#end //(End of SPF)" << endl;

    /* ---- Add comment (bounding box) ---- */
    ofs << "//\t  bounded_by {" << endl
    << "//\t   box {" << endl
    << "//\t    <"
    << min_x - MAXRADIUS <<  ","
    << min_y - MAXRADIUS <<  ","
    << min_z - MAXRADIUS << ">" << endl;

    ofs << "//\t    <"
    << max_x + MAXRADIUS << ","
    << max_y + MAXRADIUS << ","
    << max_z + MAXRADIUS << ">" << endl;

    ofs << "\t }" << endl << endl;

}

void OutputMoleculeNoBonds(ostream &ofs, string prefix)
{
    /* ---- Print description of molecule without bonds ---- */
    ofs << endl << "//Definition of Molecule " << prefix << " (no bonds)" << endl;
    ofs << "#declare " << prefix << " = object {" << prefix << "_atoms}" << endl << endl;

}

void OutputCenterComment(ostream &ofs,
                         string prefix,
                         double min_x, double max_x,
                         double min_y, double max_y,
                         double min_z, double max_z
                        )
{
    /* ---- Print center comment (Warn: Vector is multiplied by -1.0)---- */
    ofs << "//Center of molecule " << prefix << " (bounding box)" << endl;
    ofs << "#declare " << prefix << "_center = <"
    << (double) -1.0 * (min_x + max_x) / (double) 2.0 << ","
    << (double) -1.0 * (min_y + max_y) / (double) 2.0 << ","
    << (double) -1.0 * (min_z + max_z) / (double) 2.0 << ">" << endl << endl;
}

////////////////////////////////////////////////////////////////

bool PovrayFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
{
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);
    if(pmol==NULL)
        return false;

    //Define some references so we can use the old parameter names
    ostream &ofs = *pConv->GetOutStream();
    OBMol &mol = *pmol;
    const char *dimension = pConv->GetDimension();
    const char* title = pmol->GetTitle();

    static long num = 0;
    double min_x, max_x, min_y, max_y, min_z, max_z; /* Edges of bounding box */
    string prefix;

    /* ---- We use the molecule-title as our prefix ---- */
    if(title != (const char*) NULL)
        prefix = MakePrefix(title);
    else if (mol.GetTitle() != (const char *) NULL)
        prefix = MakePrefix(mol.GetTitle());
    else
        prefix = MakePrefix("Unknown");

    /* ---- Check if we have already written a molecule to this file ---- */
    if (num == 0)
    {

        /* ---- Print the header ---- */
        OutputHeader(ofs, mol, prefix);

    }
    else
    {

        /* ---- Convert the unique molecule-number to a string and set the prefix ---- */
#if defined(HAVE_SSTREAM)
        ostringstream numStr;
        numStr << num << ends;
        prefix += numStr.str().c_str();
#else

        ostrstream numStr;
        numStr << num << ends;
        prefix += numStr.str();
#endif

    }

    /* ---- Print positions and descriptions of all atoms ---- */
    OutputAtoms(ofs, mol, prefix);

    /* ---- Check #bonds ---- */
    if (mol.NumBonds() > 0)
    {

        /* ---- Write an comment ---- */
        ofs << "//Povray-description of bonds 1 - " << mol.NumBonds() << endl;

        /* ---- Do a ball and sticks model? ---- */
        ofs << "#if (BAS)" << endl;

        /* ---- Print bonds using "ball and sticks style" ---- */
        OutputBASBonds(ofs, mol, prefix);

        /* ---- End of povray-conditional for ball and sticks ---- */
        ofs << "#end //(BAS-Bonds)" << endl << endl;

        /* ---- Do a capped-sticks model? ---- */
        ofs << "#if (CST)" << endl;

        /* ---- Print bonds using "capped sticks style" ---- */
        OutputCSTBonds(ofs, mol, prefix);

        /* ---- End of povray-conditional for capped sticks ---- */
        ofs << "#end // (CST-Bonds)" << endl << endl;

    }

    /* ---- Print out unions of atoms and bonds ---- */
    OutputUnions(ofs, mol, prefix);

    /* ---- Calculate bounding-box ---- */
    CalcBoundingBox(mol, min_x, max_x, min_y, max_y, min_z, max_z);

    /* ---- Check #bonds ---- */
    if (mol.NumBonds() > 0)
    {

        /* ---- Print out description of molecule ---- */
        OutputMoleculeBonds(ofs,
                            prefix,
                            min_x, max_x,
                            min_y, max_y,
                            min_z, max_z);

    }
    else
    {

        /* ---- Now we can define the molecule without bonds ---- */
        OutputMoleculeNoBonds(ofs, prefix);

    }

    /* ---- Insert declaration for centering the molecule ---- */
    OutputCenterComment(ofs,
                        prefix,
                        min_x, max_x,
                        min_y, max_y,
                        min_z, max_z);

    /* ---- Increment the static molecule output-counter ---- */
    num++;

    /* ---- Everything is ok! ---- */
    return(true);
}

} //namespace OpenBabel
