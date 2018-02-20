#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <sstream>

#include "allreduce.hpp"

namespace {
static std::mt19937 rng_gen;
static bool rng_seeded = false;
}

template <typename Backend>
struct VectorType {
  using type = std::vector<float>;
};

template <typename Backend>
typename VectorType<Backend>::type get_vector(size_t count) {
  return typename VectorType<Backend>::type(count);
}

/** Generate random data of length count. */
template <typename Backend=allreduces::MPIBackend>
typename VectorType<Backend>::type gen_data(size_t count);

template <>
typename VectorType<allreduces::MPIBackend>::type
gen_data<allreduces::MPIBackend>(size_t count) {
  if (!rng_seeded) {
    int flag;
    MPI_Initialized(&flag);
    if (flag) {
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      rng_gen.seed(rank);
    }
  }
  std::uniform_real_distribution<float> rng;
  std::vector<float> v(count);
  for (size_t i = 0; i < count; ++i) {
    v[i] = rng(rng_gen);
  }
  return v;
}


/** Get current time. */
inline double get_time() {                                                      
  using namespace std::chrono;                                                  
  return duration_cast<duration<double>>(                                       
    steady_clock::now().time_since_epoch()).count();                            
}

/** Return a human-readable string for size. */
std::string human_readable_size(size_t size_) {
  double size = static_cast<double>(size_);
  if (size < 1024) {
    return std::to_string(size);
  }
  size /= 1024;
  if (size < 1024) {
    return std::to_string(size) + " K";
  }
  size /= 1024;
  if (size < 1024) {
    return std::to_string(size) + " M";
  }
  size /= 1024;
  return std::to_string(size) + " G";
}

template <typename Backend>
std::vector<typename Backend::algo_type> get_allreduce_algorithms() {
  std::vector<typename Backend::algo_type> algos = {
    Backend::algo_type::automatic};
  return algos;
}
 
template <>
std::vector<allreduces::MPIBackend::algo_type>
get_allreduce_algorithms<allreduces::MPIBackend>() {  
   std::vector<allreduces::AllreduceAlgorithm> algos = {
    allreduces::AllreduceAlgorithm::automatic,
    allreduces::AllreduceAlgorithm::mpi_passthrough,
    allreduces::AllreduceAlgorithm::mpi_recursive_doubling,
    allreduces::AllreduceAlgorithm::mpi_ring,
    allreduces::AllreduceAlgorithm::mpi_rabenseifner,
    allreduces::AllreduceAlgorithm::mpi_pe_ring
  };
  return algos;
}

template <typename Backend>
std::vector<typename Backend::algo_type> get_nb_allreduce_algorithms() {
  std::vector<typename Backend::algo_type> algos = {
    Backend::algo_type::automatic};
  return algos;
}
 
template <>
std::vector<allreduces::MPIBackend::algo_type>
get_nb_allreduce_algorithms<allreduces::MPIBackend>() {  
  std::vector<allreduces::AllreduceAlgorithm> algos = {
    allreduces::AllreduceAlgorithm::automatic,
    allreduces::AllreduceAlgorithm::mpi_passthrough,
    allreduces::AllreduceAlgorithm::mpi_recursive_doubling,
    allreduces::AllreduceAlgorithm::mpi_ring,
    allreduces::AllreduceAlgorithm::mpi_rabenseifner,
    //allreduces::AllreduceAlgorithm::mpi_pe_ring
  };
  return algos;
}

#define eps (1e-4)

bool check_vector(const std::vector<float>& expected,
                  const std::vector<float>& actual) {
  bool match = true;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (size_t i = 0; i < expected.size(); ++i) {
    float e = expected[i];
    if (std::abs(e - actual[i]) > eps) {
#ifdef ALUMINUM_DEBUG
      std::stringstream ss;
      ss << "[" << rank << "] @" << i << " Expected: " << e
                << ", Actual: " << actual[i] << "\n";
      std::cerr << ss.str();
      match = false;
#else
      return false;
#endif
    }
  }
  return match;
}

template <typename Backend>
typename Backend::req_type get_request();

template <>
inline typename allreduces::MPIBackend::req_type
get_request<allreduces::MPIBackend>() {
  int req = 0;
  return req;
}
