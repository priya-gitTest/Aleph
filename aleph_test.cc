#include "BoundaryMatrix.hh"
#include "Vector.hh"
#include "IO.hh"
#include "Standard.hh"
#include "PersistencePairs.hh"

#include <iostream>

using namespace aleph;
using namespace representations;

using I  = unsigned;
using V  = Vector<I>;
using BM = BoundaryMatrix<V>;
using SR = StandardReduction;

int main()
{
  auto M = load<BM>( "Triangle.txt" );

  std::cout << M << std::endl;

  computePersistencePairs<SR>( M );
}
