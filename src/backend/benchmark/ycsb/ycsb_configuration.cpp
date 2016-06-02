//===----------------------------------------------------------------------===//
//
//                         PelotonDB
//
// configuration.cpp
//
// Identification: benchmark/ycsb/configuration.cpp
//
// Copyright (c) 2015, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#undef NDEBUG

#include <iomanip>
#include <algorithm>
#include <string.h>

#include "backend/benchmark/ycsb/ycsb_configuration.h"
#include "backend/common/logger.h"

namespace peloton {
namespace benchmark {
namespace ycsb {

void Usage(FILE *out) {
  fprintf(out,
          "Command line options : ycsb <options> \n"
          "   -h --help              :  Print help message \n"
          "   -k --scale_factor      :  # of tuples \n"
          "   -d --duration          :  execution duration \n"
          "   -s --snapshot_duration :  snapshot duration \n"
          "   -b --backend_count     :  # of backends \n"
          "   -w --generate_count :  # of workload generate thread\n"
          "   -c --column_count      :  # of columns \n"
          "   -u --write_ratio       :  Fraction of updates \n"
          "   -z --zipf_theta        :  theta to control skewness \n"
          "   -m --mix_txn           :  run read/write mix txn \n"
          "   -e --exp_backoff       :  enable exponential backoff \n"
          "   -p --protocol          :  choose protocol, default OCC\n"
          "                             protocol could be occ, pcc, ssi, "
          "sread, ewrite, occrb, and to\n"
          "   -g --gc_protocol       :  choose gc protocol, default OFF\n"
          "                             gc protocol could be off, co, va\n"
          "   -q --scheduler         :  scheduler, default OFF\n"
          "                             control, queue, detect, ml");
  exit(EXIT_FAILURE);
}

static struct option opts[] = {
    {"scale_factor", optional_argument, NULL, 'k'},
    {"duration", optional_argument, NULL, 'd'},
    {"snapshot_duration", optional_argument, NULL, 's'},
    {"column_count", optional_argument, NULL, 'c'},
    {"update_ratio", optional_argument, NULL, 'u'},
    {"backend_count", optional_argument, NULL, 'b'},
    {"zipf_theta", optional_argument, NULL, 'z'},
    {"exp_backoff", no_argument, NULL, 'e'},
    {"mix_txn", no_argument, NULL, 'm'},
    {"protocol", optional_argument, NULL, 'p'},
    {"gc_protocol", optional_argument, NULL, 'g'},
    {"scheduler", optional_argument, NULL, 'q'},
    {"generate_count", optional_argument, NULL, 'w'},
    {NULL, 0, NULL, 0}};

void ValidateScaleFactor(const configuration &state) {
  if (state.scale_factor <= 0) {
    LOG_ERROR("Invalid scale_factor :: %d", state.scale_factor);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %d", "scale_factor", state.scale_factor);
}

void ValidateColumnCount(const configuration &state) {
  if (state.column_count <= 0) {
    LOG_ERROR("Invalid column_count :: %d", state.column_count);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %d", "column_count", state.column_count);
}

void ValidateUpdateRatio(const configuration &state) {
  if (state.update_ratio < 0 || state.update_ratio > 1) {
    LOG_ERROR("Invalid update_ratio :: %lf", state.update_ratio);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %lf", "update_ratio", state.update_ratio);
}

void ValidateBackendCount(const configuration &state) {
  if (state.backend_count <= 0) {
    LOG_ERROR("Invalid backend_count :: %d", state.backend_count);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %d", "backend_count", state.backend_count);
}

void ValidateDuration(const configuration &state) {
  if (state.duration <= 0) {
    LOG_ERROR("Invalid duration :: %lf", state.duration);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %lf", "execution duration", state.duration);
}

void ValidateSnapshotDuration(const configuration &state) {
  if (state.snapshot_duration <= 0) {
    LOG_ERROR("Invalid snapshot_duration :: %lf", state.snapshot_duration);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %lf", "snapshot_duration", state.snapshot_duration);
}

void ValidateZipfTheta(const configuration &state) {
  if (state.zipf_theta < 0 || state.zipf_theta > 3.0) {
    LOG_ERROR("Invalid zipf_theta :: %lf", state.zipf_theta);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %lf", "zipf_theta", state.zipf_theta);
}

void ValidateGenerateCount(const configuration &state) {
  if (state.generate_count < 0) {
    LOG_ERROR("Invalid generate_count :: %d", state.generate_count);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("%s : %d", "generate_count", state.generate_count);
}

void ParseArguments(int argc, char *argv[], configuration &state) {
  // Default Values
  state.scale_factor = 1;
  state.duration = 10;
  state.snapshot_duration = 0.1;
  state.column_count = 10;
  state.update_ratio = 0.5;
  state.backend_count = 2;
  state.zipf_theta = 0.0;
  state.generate_count = 0;  // 0 means no query thread. only prepared queries
  state.run_mix = false;
  state.run_backoff = false;
  state.protocol = CONCURRENCY_TYPE_OPTIMISTIC;
  state.scheduler = SCHEDULER_TYPE_NONE;
  state.gc_protocol = GC_TYPE_OFF;
  // Parse args
  while (1) {
    int idx = 0;
    int c = getopt_long(argc, argv, "ahmek:d:s:q:c:u:b:w:z:p:g:", opts, &idx);

    if (c == -1) break;

    switch (c) {
      case 'k':
        state.scale_factor = atoi(optarg);
        break;
      case 'd':
        state.duration = atof(optarg);
        break;
      case 's':
        state.snapshot_duration = atof(optarg);
        break;
      case 'c':
        state.column_count = atoi(optarg);
        break;
      case 'u':
        state.update_ratio = atof(optarg);
        break;
      case 'b':
        state.backend_count = atoi(optarg);
        break;
      case 'z':
        state.zipf_theta = atof(optarg);
        break;
      case 'm':
        state.run_mix = true;
        break;
      case 'e':
        state.run_backoff = true;
        break;
      case 'w':
        state.generate_count = atoi(optarg);
        break;
      case 'q': {
        char *scheduler = optarg;
        if (strcmp(scheduler, "none") == 0) {
          state.scheduler = SCHEDULER_TYPE_NONE;
        } else if (strcmp(scheduler, "control") == 0) {
          state.scheduler = SCHEDULER_TYPE_CONTROL;
        } else if (strcmp(scheduler, "queue") == 0) {
          state.scheduler = SCHEDULER_TYPE_ABORT_QUEUE;
        } else if (strcmp(scheduler, "detect") == 0) {
          state.scheduler = SCHEDULER_TYPE_CONFLICT_DETECT;
        } else if (strcmp(scheduler, "ml") == 0) {
          state.scheduler = SCHEDULER_TYPE_CONFLICT_LEANING;
        } else {
          fprintf(stderr, "\nUnknown scheduler: %s\n", scheduler);
          exit(EXIT_FAILURE);
        }
        break;
      }
      case 'p': {
        char *protocol = optarg;
        if (strcmp(protocol, "occ") == 0) {
          state.protocol = CONCURRENCY_TYPE_OPTIMISTIC;
        } else if (strcmp(protocol, "pcc") == 0) {
          state.protocol = CONCURRENCY_TYPE_PESSIMISTIC;
        } else if (strcmp(protocol, "ssi") == 0) {
          state.protocol = CONCURRENCY_TYPE_SSI;
        } else if (strcmp(protocol, "to") == 0) {
          state.protocol = CONCURRENCY_TYPE_TO;
        } else if (strcmp(protocol, "ewrite") == 0) {
          state.protocol = CONCURRENCY_TYPE_EAGER_WRITE;
        } else if (strcmp(protocol, "occrb") == 0) {
          state.protocol = CONCURRENCY_TYPE_OCC_RB;
        } else if (strcmp(protocol, "sread") == 0) {
          state.protocol = CONCURRENCY_TYPE_SPECULATIVE_READ;
        } else {
          fprintf(stderr, "\nUnknown protocol: %s\n", protocol);
          exit(EXIT_FAILURE);
        }
        break;
      }
      case 'g': {
        char *gc_protocol = optarg;
        if (strcmp(gc_protocol, "off") == 0) {
          state.gc_protocol = GC_TYPE_OFF;
        } else if (strcmp(gc_protocol, "va") == 0) {
          state.gc_protocol = GC_TYPE_VACUUM;
        } else if (strcmp(gc_protocol, "co") == 0) {
          state.gc_protocol = GC_TYPE_CO;
        } else {
          fprintf(stderr, "\nUnknown gc protocol: %s\n", gc_protocol);
          exit(EXIT_FAILURE);
        }
        break;
      }
      case 'h':
        Usage(stderr);
        exit(EXIT_FAILURE);
        break;
      default:
        fprintf(stderr, "\nUnknown option: -%c-\n", c);
        Usage(stderr);
        exit(EXIT_FAILURE);
    }
  }

  // Print configuration
  ValidateScaleFactor(state);
  ValidateColumnCount(state);
  ValidateUpdateRatio(state);
  ValidateBackendCount(state);
  ValidateDuration(state);
  ValidateSnapshotDuration(state);
  ValidateZipfTheta(state);
  ValidateGenerateCount(state);

  LOG_INFO("%s : %d", "Run mix query", state.run_mix);
  LOG_INFO("%s : %d", "Run exponential backoff", state.run_backoff);
}

}  // namespace ycsb
}  // namespace benchmark
}  // namespace peloton
