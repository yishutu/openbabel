#include "obtest.h"
#include <openbabel/mol.h>
#include <openbabel/obconversion.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;
using namespace OpenBabel;


// A segfault was occuring when a Universal SMILES was output after an InChIfied SMILES.
// This was due to short-circuit caching of InChIs on reading. The fix was to limit
// the situations when the cached value was used, but also to delete the cached value
// in this particular instance.
void test_Issue135_UniversalSmiles()
{
  // Test writing U smiles after I smiles
  OBConversion conv;
  conv.SetInFormat("smi");
  OBMol mol;
  conv.ReadString(&mol, "C(=O)([O-])C(=O)O");
  conv.SetOutFormat("smi");
  conv.SetOptions("I", OBConversion::OUTOPTIONS);
  std::string res = conv.WriteString(&mol, true);
  OB_COMPARE(res, "C(=O)(C(=O)O)[O-]");
  conv.SetOptions("U", OBConversion::OUTOPTIONS);
  res = conv.WriteString(&mol, true);
  OB_COMPARE(res, "C(=O)(C(=O)[O-])O");
}

// Reading an InChI and then adding hydrogens messed up the structure
void test_Issue134_InChI_addH()
{
  OBConversion conv;
  conv.SetInFormat("inchi");
  OBMol mol;
  conv.ReadString(&mol, "InChI=1S/C2H7NO/c1-2(3)4/h2,4H,3H2,1H3/t2-/m0/s1");
  OB_ASSERT(!mol.HasData(OBGenericDataType::VirtualBondData));
  mol.AddHydrogens();
  conv.SetOutFormat("smi");
  std::string res = conv.WriteString(&mol, true);
  OB_COMPARE(res, "C[C@@H](N)O");
}

// Delete hydrogens should not remove isotopic hydrogens or [H][H] or [Cu][H][Cu]
void test_Issue178_DeleteHudrogens()
{
  OBConversion conv;
  conv.SetInFormat("smi");
  OBMol mol;
  // Test DeleteHydrogens() and DeleteNonPolarHydrogens()
  static const char *smi[] = { "C[H]", "[H][H]", "C[1H]", "C[H]C" };
  int numHs[] = { 0, 2, 1, 1 };
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 2; ++j) {
      conv.ReadString(&mol, smi[i]);
      if (j == 0)
        mol.DeleteHydrogens();
      else
        mol.DeleteNonPolarHydrogens();
      int myNumHs = 0;
      FOR_ATOMS_OF_MOL(atom, mol)
        if (atom->IsHydrogen())
          myNumHs++;
      OB_COMPARE(myNumHs, numHs[i]);
    }
  }
  // Test DeletePolarHydrogens()
  static const char *smiB[] = { "N[H]", "[H][H]", "N[1H]", "N[H]C" };
  int numHsB[] = { 0, 2, 1, 1 };
  for (int i = 0; i < 4; ++i) {
    conv.ReadString(&mol, smiB[i]);
    mol.DeletePolarHydrogens();
    int myNumHs = 0;
    FOR_ATOMS_OF_MOL(atom, mol)
      if (atom->IsHydrogen())
        myNumHs++;
    OB_COMPARE(myNumHs, numHsB[i]);
  }  
}

int regressionstest(int argc, char* argv[])
{
  int defaultchoice = 1;
  
  int choice = defaultchoice;

  if (argc > 1) {
    if(sscanf(argv[1], "%d", &choice) != 1) {
      printf("Couldn't parse that input as a number\n");
      return -1;
    }
  }
  // Define location of file formats for testing
  #ifdef FORMATDIR
    char env[BUFF_SIZE];
    snprintf(env, BUFF_SIZE, "BABEL_LIBDIR=%s", FORMATDIR);
    putenv(env);
  #endif  

  switch(choice) {
  case 1:
    test_Issue135_UniversalSmiles();
    break;
  case 221:
    test_Issue134_InChI_addH();
    break;
  case 222:
    test_Issue178_DeleteHudrogens();
    break;
    //case N:
  //  YOUR_TEST_HERE();
  //  Remember to update CMakeLists.txt with the number of your test
  //  break;
  default:
    cout << "Test number " << choice << " does not exist!\n";
    return -1;
  }

  return 0;
}

