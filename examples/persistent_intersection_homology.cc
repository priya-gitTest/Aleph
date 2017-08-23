#include <aleph/containers/DimensionalityEstimators.hh>
#include <aleph/containers/PointCloud.hh>

#include <aleph/config/FLANN.hh>

#ifdef ALEPH_WITH_FLANN
  #include <aleph/geometry/FLANN.hh>
#else
  #include <aleph/geometry/BruteForce.hh>
#endif

#include <aleph/geometry/SphereSampling.hh>
#include <aleph/geometry/VietorisRipsComplex.hh>

#include <aleph/geometry/distances/Euclidean.hh>

#include <aleph/persistentHomology/Calculation.hh>
#include <aleph/persistentHomology/PhiPersistence.hh>

#include <aleph/topology/BarycentricSubdivision.hh>
#include <aleph/topology/Simplex.hh>
#include <aleph/topology/SimplicialComplex.hh>
#include <aleph/topology/Skeleton.hh>

#include <aleph/topology/filtrations/Data.hh>

#include <fstream>
#include <iostream>

using DataType           = double;
using VertexType         = unsigned;
using Distance           = aleph::distances::Euclidean<DataType>;
using PointCloud         = aleph::containers::PointCloud<DataType>;
using Simplex            = aleph::topology::Simplex<DataType, VertexType>;
using SimplicialComplex  = aleph::topology::SimplicialComplex<Simplex>;
using Filtration         = aleph::topology::filtrations::Data<Simplex>;
using PersistenceDiagram = aleph::PersistenceDiagram<DataType>;

#ifdef ALEPH_WITH_FLANN
  using NearestNeighbours = aleph::geometry::FLANN<PointCloud, Distance>;
#else
  using NearestNeighbours = aleph::geometry::BruteForce<PointCloud, Distance>;
#endif

PointCloud makeOnePointUnionOfSpheres( unsigned n )
{
  auto makeSphere = [] ( unsigned n, DataType r, DataType x0, DataType y0, DataType z0 )
  {
    auto angles     = aleph::geometry::sphereSampling<DataType>( n );
    auto pointCloud = aleph::geometry::makeSphere( angles, r, x0, y0, z0 );

    return pointCloud;
  };

  auto sphere1 = makeSphere( n, DataType(1), DataType(0), DataType(0), DataType(0) );
  auto sphere2 = makeSphere( n, DataType(1), DataType(2), DataType(0), DataType(0) );

  return sphere1 + sphere2;
}

PointCloud makeTwoSpheres( unsigned n )
{
  auto makeSphere = [] ( unsigned n, DataType r, DataType x0, DataType y0, DataType z0 )
  {
    auto angles     = aleph::geometry::sphereSampling<DataType>( n );
    auto pointCloud = aleph::geometry::makeSphere( angles, r, x0, y0, z0 );

    return pointCloud;
  };

  auto sphere1 = makeSphere( n, DataType(1), DataType(0), DataType(0), DataType(0) );
  auto sphere2 = makeSphere( n, DataType(1), DataType(3), DataType(0), DataType(0) );

  return sphere1 + sphere2;
}

int main(int, char**)
{
  auto pointCloud       = makeOnePointUnionOfSpheres(500);
  auto dimensionalities = aleph::containers::estimateLocalDimensionalityNearestNeighbours<Distance, PointCloud, NearestNeighbours>( pointCloud, 10 );

  {
    std::ofstream out1( "/tmp/P.txt" );
    std::ofstream out2( "/tmp/F.txt" );

    out1 << pointCloud << "\n";

    for( auto&& dimensionality : dimensionalities )
      out2 << dimensionality << "\n";
  }

  auto K
    = aleph::geometry::buildVietorisRipsComplex(
        NearestNeighbours( pointCloud ),
        DataType( 0.25 ),
        2
  );

  // Skeleta (or skeletons?)
  auto K0 = aleph::topology::Skeleton()( 0, K );
  auto K1 = aleph::topology::Skeleton()( 1, K );
  auto K2 = aleph::topology::Skeleton()( 2, K );

  // Barycentric subdivision to ensure that the resulting complex is
  // flaglike in sense of MacPherson et al.
  auto L
    = aleph::topology::BarycentricSubdivision()( K, [] ( std::size_t dimension ) { return dimension == 0 ? 1 : 0.5; } );

  L.recalculateWeights();
  L.sort( aleph::topology::filtrations::Data<typename decltype(L)::ValueType>() ); // FIXME

  auto D1 = aleph::calculateIntersectionHomology( L, {K0,K1,K2}, aleph::Perversity( {-1, 0} ) );
  auto D2 = aleph::calculateIntersectionHomology( L, {K0,K1,K2}, aleph::Perversity( {-1, 1} ) );
  auto D3 = aleph::calculateIntersectionHomology( L, {K0,K1,K2}, aleph::Perversity( { 0, 0} ) );
  auto D4 = aleph::calculateIntersectionHomology( L, {K0,K1,K2}, aleph::Perversity( { 0, 1} ) );
  auto D5 = aleph::calculatePersistenceDiagrams ( L );

  std::vector<PersistenceDiagram> persistenceDiagrams;
  persistenceDiagrams.reserve( D1.size() + D2.size() + D3.size() + D4.size() + D5.size() );

  for( auto&& D : {D1,D2,D3,D4,D5} )
    persistenceDiagrams.insert( persistenceDiagrams.end(), D.begin(), D.end() );

  {
    std::ofstream out0( "/tmp/D_0.txt" );
    std::ofstream out1( "/tmp/D_1.txt" );

    for( auto&& D : persistenceDiagrams )
    {
      D.removeDiagonal();

      if( D.dimension() == 0 )
        out0 << D << "\n\n";
      else if( D.dimension() == 1 )
        out1 << D << "\n\n";
    }
  }
}
