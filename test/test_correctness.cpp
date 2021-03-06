////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  Produced at the
// Lawrence Livermore National Laboratory in collaboration with University of
// Illinois Urbana-Champaign.
//
// Written by the LBANN Research Team (N. Dryden, N. Maruyama, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-756777.
// All rights reserved.
//
// This file is part of Aluminum GPU-aware Communication Library. For details, see
// http://software.llnl.gov/Aluminum or https://github.com/LLNL/Aluminum.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include "Al.hpp"
#include "test_utils.hpp"
#ifdef AL_HAS_NCCL
#include "test_utils_nccl_cuda.hpp"
#endif
#ifdef AL_HAS_MPI_CUDA
#include "test_utils_mpi_cuda.hpp"
#endif

#include <stdlib.h>
#include <math.h>
#include <string>

size_t max_size = 1<<30;

void get_expected_result(std::vector<float>& expected) {
  MPI_Allreduce(MPI_IN_PLACE, expected.data(), expected.size(),
                MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);
}

/**
 * Test allreduce algo on input, check with expected.
 */
template <typename Backend>
void test_allreduce_algo(const typename VectorType<Backend>::type& expected,
                         typename VectorType<Backend>::type input,
                         typename Backend::comm_type& comm,
                         typename Backend::algo_type algo) {
  auto recv = get_vector<Backend>(input.size());
  // Test regular allreduce.
  Al::Allreduce<Backend>(input.data(), recv.data(), input.size(),
                         Al::ReductionOperator::sum, comm, algo);
  if (!check_vector(expected, recv)) {
    std::cout << comm.rank() << ": regular allreduce does not match" <<
        std::endl;
    std::abort();
  }
  MPI_Barrier(MPI_COMM_WORLD);
  // Test in-place allreduce.
  Al::Allreduce<Backend>(input.data(), input.size(),
                         Al::ReductionOperator::sum, comm, algo);
  if (!check_vector(expected, input)) {
    std::cout << comm.rank() << ": in-place allreduce does not match" <<
      std::endl;
    std::abort();
  }
}

/**
 * Test non-blocking allreduce algo on input, check with expected.
 */
template <typename Backend>
void test_nb_allreduce_algo(const typename VectorType<Backend>::type& expected,
                            typename VectorType<Backend>::type input,
                            typename Backend::comm_type& comm,
                            typename Backend::algo_type algo) {
  typename Backend::req_type req = get_request<Backend>();
  auto recv = get_vector<Backend>(input.size());
  // Test regular allreduce.
  Al::NonblockingAllreduce<Backend>(input.data(), recv.data(), input.size(),
                                    Al::ReductionOperator::sum, comm,
                                    req, algo);
  Al::Wait<Backend>(req);
  if (!check_vector(expected, recv)) {
    std::cout << comm.rank() << ": regular allreduce does not match" <<
      std::endl;
    std::abort();
  }
  MPI_Barrier(MPI_COMM_WORLD);
  // Test in-place allreduce.
  Al::NonblockingAllreduce<Backend>(input.data(), input.size(),
                                    Al::ReductionOperator::sum, comm,
                                    req, algo);
  Al::Wait<Backend>(req);
  if (!check_vector(expected, input)) {
    std::cout << comm.rank() << ": in-place allreduce does not match" <<
      std::endl;
    std::abort();
  }
}

template <typename Backend>
void test_correctness() {
  auto algos = get_allreduce_algorithms<Backend>();
  auto nb_algos = get_nb_allreduce_algorithms<Backend>();
  typename Backend::comm_type comm;  // Use COMM_WORLD.
  // Compute sizes to test.
  std::vector<size_t> sizes = {0};
  for (size_t size = 1; size <= max_size; size *= 2) {
    sizes.push_back(size);
    // Avoid duplicating 2.
    if (size > 1) {
      sizes.push_back(size + 1);
    }
  }
  for (const auto& size : sizes) {
    if (comm.rank() == 0) {
      std::cout << "Testing size " << human_readable_size(size) << std::endl;
    }
    // Compute true value.
    typename VectorType<Backend>::type &&data = gen_data<Backend>(size);
    auto expected(data);
    get_expected_result(expected);
    // Test algorithms.
    for (auto&& algo : algos) {
      MPI_Barrier(MPI_COMM_WORLD);
      if (comm.rank() == 0) {
        std::cout << " Algo: " << Al::allreduce_name(algo) << std::endl;
      }
      test_allreduce_algo<Backend>(expected, data, comm, algo);
    }
    for (auto&& algo : nb_algos) {
      MPI_Barrier(MPI_COMM_WORLD);
      if (comm.rank() == 0) {
        std::cout << " Algo: NB " << Al::allreduce_name(algo) << std::endl;
      }
      test_nb_allreduce_algo<Backend>(expected, data, comm, algo);
    }
  }
}

int main(int argc, char** argv) {
  // Need to set the CUDA device before initializing Aluminum.
#ifdef AL_HAS_CUDA
  set_device();
#endif
  Al::Initialize(argc, argv);

  std::string backend = "MPI";
  if (argc >= 2) {
    backend = argv[1];
  }
  if (argc == 3) {
    max_size = std::stoul(argv[2]);
  }

  if (backend == "MPI") {
    test_correctness<Al::MPIBackend>();
#ifdef AL_HAS_NCCL
  } else if (backend == "NCCL") {
    test_correctness<Al::NCCLBackend>();
#endif
#ifdef AL_HAS_MPI_CUDA
  } else if (backend == "MPI-CUDA") {
    test_correctness<Al::MPICUDABackend>();
#endif
  } else {
    std::cerr << "usage: " << argv[0] << " [MPI";
#ifdef AL_HAS_NCCL
    std::cerr << " | NCCL";
#endif
#ifdef AL_HAS_MPI_CUDA
    std::cerr << " | MPI-CUDA";
#endif
    std::cerr << "]" << std::endl;
    return -1;
  }

  Al::Finalize();
  return 0;
}
