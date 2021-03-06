#ifndef ALEPH_CONTAINERS_DIMENSIONALITY_ESTIMATORS_HH__
#define ALEPH_CONTAINERS_DIMENSIONALITY_ESTIMATORS_HH__

#include <aleph/math/KahanSummation.hh>
#include <aleph/math/PrincipalComponentAnalysis.hh>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>

#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

#include <cmath>

namespace aleph
{

namespace containers
{

/**
  Estimates local intrinsic dimensionality of a container using its
  nearest neighbours. The underlying assumption of the estimator is
  that points are locally uniformly distributed uniformly. Use this
  estimator with care when analysing unknown data.

  @param container Container to use for dimensionality estimation
  @param k         Number of nearest neighbours
  @param distance  Distance measure

  @returns Vector of local intrinsic dimensionality estimates. Note
           that the numbers are reported *without* rounding.
*/

template <
  class Distance,
  class Container,
  class Wrapper
> std::vector<double> estimateLocalDimensionalityNearestNeighbours( const Container& container,
                                                                    unsigned k,
                                                                    Distance /* distance */ = Distance() )
{
  using IndexType   = typename Wrapper::IndexType;
  using ElementType = typename Wrapper::ElementType;

  std::vector< std::vector<IndexType> > indices;
  std::vector< std::vector<ElementType> > distances;

  Wrapper nnWrapper( container );
  nnWrapper.neighbourSearch( k+1, indices, distances );

  auto n = container.size();

  std::vector<double> estimates;
  estimates.reserve( n );

  for( decltype(n) i = 0; i < n; i++ )
  {
    auto&& nnDistances = distances.at(i);
    auto r1            = aleph::math::accumulate_kahan( nnDistances.begin(), nnDistances.begin() + k,     0.0 ) / static_cast<double>(k  );
    auto r2            = aleph::math::accumulate_kahan( nnDistances.begin(), nnDistances.begin() + k + 1, 0.0 ) / static_cast<double>(k+1);

    estimates.push_back( r1 / ( (r2-r1)*k ) );
  }

  return estimates;
}

/**
  Estimates local intrinsic dimensionality of a container using its
  nearest neighbours. No assumptions about the distribution of data
  points are made. The function uses an iteration over a *range* of
  nearest neighbours and solves a regression problem.

  Please see the publication

    > An evaluation of intrinsic dimensionality estimators\n
    > Peter J. Verveer and Robert P. W. Duin\n
    > IEEE Transactions on Pattern Analysis and Machine Intelligence 17.1, pp. 81-86, 1985

  for more details.

  @param container Container to use for dimensionality estimation

  @param kMin      Minimum number of nearest neighbours to use in
                   computing local dimensionality estimates.

  @param kMax      Maximum number of nearest neighbours to use in
                   computing local dimensionality estimates. This
                   parameter influences performance.

  @param distance  Distance measure

  @returns Vector of local intrinsic dimensionality estimates. Note that
           the numbers are reported *without* rounding.
*/

template <
  class Distance,
  class Container,
  class Wrapper
> std::vector<double> estimateLocalDimensionalityNearestNeighbours( const Container& container,
                                                                    unsigned kMin,
                                                                    unsigned kMax,
                                                                    Distance /* distance */ = Distance() )

{
  if( kMin > kMax )
    std::swap( kMin, kMax );

  if( kMax == 0 || kMin == 0 )
    throw std::runtime_error( "Expecting non-zero number of nearest neighbours" );

  using IndexType   = typename Wrapper::IndexType;
  using ElementType = typename Wrapper::ElementType;

  std::vector< std::vector<IndexType> > indices;
  std::vector< std::vector<ElementType> > distances;

  Wrapper nnWrapper( container );
  nnWrapper.neighbourSearch( kMax, indices, distances );

  auto n = container.size();

  std::vector<double> estimates;
  estimates.reserve( n );

  for( decltype(n) i = 0; i < n; i++ )
  {
    auto&& nnDistances = distances.at(i);

    std::vector<double> localEstimates;
    localEstimates.reserve( kMax );

    for( unsigned k = kMin; k < kMax; k++ )
    {
      auto r = aleph::math::accumulate_kahan( nnDistances.begin(), nnDistances.begin() + k, 0.0 ) / static_cast<double>(k);
      localEstimates.emplace_back( r );
    }

    // The dimensionality estimates consist of two terms. The first term
    // is similar to the local biased dimensionality estimate.

    std::vector<double> firstTerms;
    firstTerms.reserve( kMax );

    std::vector<double> secondTerms;
    secondTerms.reserve( kMax );

    for( unsigned k = kMin; k < kMax - 1; k++ )
    {
      auto index = k - kMin;
      auto r1    = localEstimates.at(index);
      auto r2    = localEstimates.at(index+1);

      firstTerms.emplace_back ( ( (r2-r1) * r1 ) / k );
      secondTerms.emplace_back( ( (r2-r1) * (r2-r1) ) );
    }

    auto s = aleph::math::accumulate_kahan( firstTerms.begin() , firstTerms.end() , 0.0 );
    auto t = aleph::math::accumulate_kahan( secondTerms.begin(), secondTerms.end(), 0.0 );

    estimates.push_back( s / t );
  }

  return estimates;

}

/**
  Estimates local intrinsic dimensionality of a container using its
  nearest neighbours. No assumptions about the distribution of data
  points are made. The function uses *maximum likelihood estimates*
  for the dimensionality estimates.

  Please see the publication

    > Maximum Likelihood Estimation of Intrinsic Dimension\n
    > Elizaveta Levina and Peter J. Bickel\n
    > Advances in Neural Information Processing Systems, 2005

  for more details.

  @param container Container to use for dimensionality estimation

  @param kMin      Minimum number of nearest neighbours to use in
                   computing local dimensionality estimates.

  @param kMax      Maximum number of nearest neighbours to use in
                   computing local dimensionality estimates. This
                   parameter influences performance.

  @param distance  Distance measure

  @returns Vector of local intrinsic dimensionality estimates. Note that
           the numbers are reported *without* rounding.
*/

template <
  class Distance,
  class Container,
  class Wrapper
> std::vector<double> estimateLocalDimensionalityNearestNeighboursMLE( const Container& container,
                                                                       unsigned kMin,
                                                                       unsigned kMax,
                                                                       Distance /* distance */ = Distance() )
{
  if( kMin > kMax )
    std::swap( kMin, kMax );

  if( kMax == 0 || kMin == 0 )
    throw std::runtime_error( "Expecting non-zero number of nearest neighbours" );

  using IndexType   = typename Wrapper::IndexType;
  using ElementType = typename Wrapper::ElementType;

  std::vector< std::vector<IndexType> > indices;
  std::vector< std::vector<ElementType> > distances;

  Wrapper nnWrapper( container );
  nnWrapper.neighbourSearch( kMax, indices, distances );

  auto n = container.size();

  std::vector<double> estimates;
  estimates.reserve( n );

  for( decltype(n) i = 0; i < n; i++ )
  {
    auto&& nnDistances = distances.at(i);

    std::vector<double> localEstimates;
    localEstimates.reserve( kMax );

    // This follows the notation in the original paper. I dislike using
    // $T_k$ to denote distances, though.
    for( unsigned k = kMin - 1; k < kMax; k++ )
    {
      // Nothing to do here...
      if( k == 0 )
        continue;

      std::vector<double> logEstimates;
      logEstimates.reserve( k-1 );

      for( auto it = nnDistances.begin(); it != nnDistances.begin() + k; ++it )
      {
        if( *it > 0.0 && nnDistances.at(k) > 0.0 )
          logEstimates.push_back( std::log( nnDistances.at(k) / *it ) );

        // This defines log(0) = 0, as usually done in information
        // theory. The original paper does not handle this.
        else
          logEstimates.push_back( 0.0 );
      }

      auto mk = k > 1 ? 1.0 / (k-1) * aleph::math::accumulate_kahan( logEstimates.begin(), logEstimates.end(), 0.0 )
                      : 0.0;

      if( mk > 0.0 )
        mk = 1.0 / mk;
      else
        mk = 0.0;

      localEstimates.push_back( mk );
    }

    estimates.push_back( aleph::math::accumulate_kahan( localEstimates.begin(), localEstimates.end(), 0.0 ) / (kMax - kMin + 1) );
  }

  return estimates;
}

/**
  Estimates local intrinsic dimensionality of a container using its
  minimum spanning tree.

  @param container Container to use for dimensionality estimation
  @param distance  Distance measure

  @returns Vector of local intrinsic dimensionality estimates. Note
           that the numbers are reported *without* rounding.
*/

template <
  class Distance,
  class Container
> std::vector<double> estimateLocalDimensionalityNearestNeighbours( const Container& container,
                                                                    Distance distance = Distance() )
{
  using WeightedGraph
    = boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::undirectedS,
        boost::no_property,
        boost::property<boost::edge_weight_t, double> >;

  using VertexDescriptor = boost::graph_traits<WeightedGraph>::vertex_descriptor;
  using EdgeDescriptor   = boost::graph_traits<WeightedGraph>::edge_descriptor;

  auto n = container.size();
  auto d = container.dimension();

  WeightedGraph G( n );

  for( std::size_t i = 0; i < n; i++ )
  {
    auto&& p = container[i];

    for( std::size_t j = i+1; j < n; j++ )
    {
      auto&& q  = container[j];
      auto dist = distance( p.begin(), q.begin(), d );

      boost::add_edge( VertexDescriptor(i),
                       VertexDescriptor(j),
                       dist,
                       G );
    }
  }

  std::vector<EdgeDescriptor> mstEdges;
  boost::kruskal_minimum_spanning_tree( G,
                                        std::back_inserter( mstEdges ) );

  WeightedGraph MST( n );

  for( auto&& edge : mstEdges )
  {
    boost::add_edge( boost::source( edge, G ),
                     boost::target( edge, G ),
                     MST );
  }

  for( auto pair = boost::vertices( MST ); pair.first != pair.second; ++pair.first )
  {
  }

  return {};
}

/**
  Estimates local intrinsic dimensionality of a container using its
  local principal components. The basic premise is that the largest
  spectral gap in the eigenspectrum of a local PCA gives a suitable
  hint about the local dimensionality at the given set of points.

  @param container Container to use for dimensionality estimation
  @param distance  Distance measure

  @returns Vector of local intrinsic dimensionality estimates. The numbers are
           not rounded but taken directly from the estimation procedure.
*/

template <
  class Distance,
  class Container,
  class Wrapper
> std::vector<unsigned> estimateLocalDimensionalityPCA( const Container& container,
                                                        unsigned k,
                                                        Distance /* distance */ = Distance() )
{
  using IndexType   = typename Wrapper::IndexType;
  using ElementType = typename Wrapper::ElementType;

  std::vector< std::vector<IndexType> > indices;
  std::vector< std::vector<ElementType> > distances;

  Wrapper nnWrapper( container );
  nnWrapper.neighbourSearch( k+1, indices, distances );

  std::vector<unsigned> estimates;
  estimates.reserve( container.size() );

  for( auto&& localIndices : indices )
  {
    std::vector< std::vector<ElementType> > data( localIndices.size(), std::vector<ElementType>() );

    {
      std::size_t i = 0;
      for( auto&& index : localIndices )
      {
        auto&& p = container[index];

        data[i].assign( p.begin(), p.end() );

        i++;
      }
    }

    // Calculate a (local) principal component analysis and analyse the
    // resulting spectrum. The largest spectral gap is used to estimate
    // the local intrinsic dimensionality.
    aleph::math::PrincipalComponentAnalysis pca;
    auto result           = pca( data );
    auto&& singularValues = result.singularValues;

    if( singularValues.size() >= 2 )
    {
      ElementType spectralGap = std::numeric_limits<ElementType>::lowest();
      unsigned spectralIndex  = 0;

      auto prev = singularValues.begin();
      auto curr = std::next( prev );

      for( ; curr != singularValues.end(); ++prev, ++curr)
      {
        auto gap = std::abs( *prev - *curr );

        if( gap > spectralGap )
        {
          // Notice that I am using the *lower* bound of the index here
          // by specifying `prev` instead of current. This makes sense,
          // as a jumping between $i-1$ and $i$ indicates that $i-1$ is
          // sufficient to describe the data adequately.
          //
          // The additional offset of 1 is used because `std::distance`
          // uses zero-based indices.
          spectralGap   = gap;
          spectralIndex = static_cast<unsigned>( std::distance( singularValues.begin(), prev ) ) + 1;
        }
      }

      estimates.push_back( spectralIndex );
    }
  }

  return estimates;
}

} // namespace containers

} // namespace aleph

#endif
