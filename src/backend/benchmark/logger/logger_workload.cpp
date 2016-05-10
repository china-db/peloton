//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// logger_workload.cpp
//
// Identification: src/backend/benchmark/logger/logger_workload.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <thread>
#include <string>
#include <getopt.h>
#include <sys/stat.h>

#undef NDEBUG

#include "backend/bridge/ddl/ddl_database.h"
#include "backend/concurrency/transaction_manager_factory.h"
#include "backend/common/value_factory.h"

#include "backend/common/exception.h"
#include "backend/common/logger.h"
#include "backend/common/timer.h"
#include "backend/logging/log_manager.h"

#include "backend/benchmark/logger/logger_workload.h"
#include "backend/benchmark/logger/logger_loader.h"
#include "backend/benchmark/ycsb/ycsb_workload.h"
#include "backend/benchmark/ycsb/ycsb_configuration.h"
#include "backend/benchmark/ycsb/ycsb_loader.h"

#include <unistd.h>

//===--------------------------------------------------------------------===//
// GUC Variables
//===--------------------------------------------------------------------===//

namespace peloton {
namespace benchmark {
namespace logger {

//===--------------------------------------------------------------------===//
// PREPARE LOG FILE
//===--------------------------------------------------------------------===//

//===--------------------------------------------------------------------===//
// 1. Standby -- Bootstrap
// 2. Recovery -- Optional
// 3. Logging -- Collect data and flush when commit
// 4. Terminate -- Collect any remaining data and flush
// 5. Sleep -- Disconnect backend loggers and frontend logger from manager
//===--------------------------------------------------------------------===//

#define LOGGING_TESTS_DATABASE_OID 20000
#define LOGGING_TESTS_TABLE_OID 10000

std::ofstream out("outputfile.summary");

size_t GetLogFileSize();

static void WriteOutput(double value) {
	std::cout << "----------------------------------------------------------\n";
	std::cout << state.benchmark_type << " "
	    << state.logging_type << " "
	    << ycsb::state.update_ratio << " "
	    << ycsb::state.scale_factor << " "
	    << ycsb::state.backend_count << " "
      << ycsb::state.skew_factor << " "
      << ycsb::state.duration << " "
      << state.nvm_latency << " "
      << state.pcommit_latency << " "
      << state.flush_mode << " "
      << state.asynchronous_mode << " "
      " :: ";
	std::cout << value << "\n";

  out << state.benchmark_type << " ";
	out << state.logging_type << " ";
	out << ycsb::state.update_ratio << " ";
	out << ycsb::state.scale_factor << " ";
  out << ycsb::state.backend_count << " ";
  out << ycsb::state.skew_factor << " ";
  out << ycsb::state.duration << " ";
  out << state.nvm_latency << " ";
  out << state.pcommit_latency << " ";
  out << state.flush_mode << " ";
  out << state.asynchronous_mode << " ";
  out << value << "\n";
	out.flush();
}

std::string GetFilePath(std::string directory_path, std::string file_name) {
  std::string file_path = directory_path;

  // Add a trailing slash to a file path if needed
  if (!file_path.empty() && file_path.back() != '/') file_path += '/';

  file_path += file_name;

  return file_path;
}

/**
 * @brief writing a simple log file
 */
bool PrepareLogFile() {
  if (chdir(state.log_file_dir.c_str())) {
    LOG_ERROR("change directory failed");
  }

  // start a thread for logging
  auto& log_manager = logging::LogManager::GetInstance();
  log_manager.SetLogFileName("wbl.log");
  if (log_manager.ContainsFrontendLogger() == true) {
    LOG_ERROR("another logging thread is running now");
    return false;
  }

        // Get an instance of the storage manager to force posix_fallocate
        auto &storage_manager = storage::StorageManager::GetInstance();
        auto msync_count = storage_manager.GetMsyncCount();
        printf("msync count : %lu\n", msync_count);
        

  Timer<> timer;
  std::thread thread;


  if (peloton_logging_mode != LOGGING_TYPE_INVALID) {
    // Launching a thread for logging
    if (!log_manager.IsInLoggingMode()) {
      // Set sync commit mode
      log_manager.SetSyncCommit(false);

      // Wait for standby mode
      auto local_thread = std::thread(
          &peloton::logging::LogManager::StartStandbyMode, &log_manager);
      thread.swap(local_thread);
      log_manager.WaitForModeTransition(peloton::LOGGING_STATUS_TYPE_STANDBY,
                                        true);

      // Clean up database tile state before recovery from checkpoint
      log_manager.PrepareRecovery();

      // Do any recovery
      log_manager.StartRecoveryMode();

      // Wait for logging mode
      log_manager.WaitForModeTransition(peloton::LOGGING_STATUS_TYPE_LOGGING,
                                        true);

      // Done recovery
      log_manager.DoneRecovery();
    }
  }

  timer.Start();

  // Build the log
  BuildLog();

  // Stop frontend logger if in a valid logging mode
  if (peloton_logging_mode != LOGGING_TYPE_INVALID) {
    //  Wait for the mode transition :: LOGGING -> TERMINATE -> SLEEP
    if (log_manager.EndLogging()) {
      thread.join();
    }
  }

  timer.Stop();

  auto duration = timer.GetDuration();
  auto throughput =
      (ycsb::state.duration * ycsb::state.backend_count) / duration;

  // Log the build log time
  if (state.experiment_type == EXPERIMENT_TYPE_INVALID ||
      state.experiment_type == EXPERIMENT_TYPE_THROUGHPUT) {
    WriteOutput(throughput);
  } else if (state.experiment_type == EXPERIMENT_TYPE_STORAGE) {
    // TODO: Need to get more info
    // in case of WAL: checkpoint size
    // in case of WBL: table size, index size ?
    auto log_file_size = GetLogFileSize();
    WriteOutput(log_file_size);
  }
  else if(state.experiment_type == EXPERIMENT_TYPE_LATENCY) {
    // TODO: Fix this
    WriteOutput(ycsb::state.flush_frequency);
  }

  return true;
}

//===--------------------------------------------------------------------===//
// CHECK RECOVERY
//===--------------------------------------------------------------------===//

void ResetSystem() {
  // XXX Initialize oid since we assume that we restart the system

  auto& txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  txn_manager.ResetStates();
}

/**
 * @brief recover the database and check the tuples
 */
void DoRecovery(std::string file_name) {
  auto file_path = GetFilePath(state.log_file_dir, file_name);

  std::ifstream log_file(file_path);

  // Reset the log file if exists
  log_file.close();

  ycsb::CreateYCSBDatabase();

  //===--------------------------------------------------------------------===//
  // RECOVERY
  //===--------------------------------------------------------------------===//

  Timer<std::milli> timer;
  std::thread thread;

  timer.Start();

  auto& log_manager = peloton::logging::LogManager::GetInstance();
  log_manager.ResetLogStatus();
  log_manager.ResetFrontendLoggers();
  if (peloton_logging_mode != LOGGING_TYPE_INVALID) {
    // Launching a thread for logging
    if (!log_manager.IsInLoggingMode()) {
      // Set sync commit mode
      log_manager.SetSyncCommit(false);

      // Wait for standby mode
      auto local_thread = std::thread(
          &peloton::logging::LogManager::StartStandbyMode, &log_manager);
      thread.swap(local_thread);
      log_manager.WaitForModeTransition(peloton::LOGGING_STATUS_TYPE_STANDBY,
                                        true);

      // Clean up database tile state before recovery from checkpoint
      log_manager.PrepareRecovery();

      // Do any recovery
      log_manager.StartRecoveryMode();

      // Wait for logging mode
      log_manager.WaitForModeTransition(peloton::LOGGING_STATUS_TYPE_LOGGING,
                                        true);

      // Done recovery
      log_manager.DoneRecovery();
    }
  }

  if (log_manager.EndLogging()) {
    thread.join();
  } else {
    LOG_ERROR("Failed to terminate logging thread");
  }

  timer.Stop();

  // Recovery time (in ms)

  WriteOutput(timer.GetDuration());
}

//===--------------------------------------------------------------------===//
// WRITING LOG RECORD
//===--------------------------------------------------------------------===//

void BuildLog() {
  ycsb::CreateYCSBDatabase();

  ycsb::LoadYCSBDatabase();

  //===--------------------------------------------------------------------===//
  // ACTIVE PROCESSING
  //===--------------------------------------------------------------------===//
  ycsb::RunWorkload();
}

size_t GetLogFileSize() {
  struct stat log_stats;

  auto& log_manager = logging::LogManager::GetInstance();
  std::string log_file_name = log_manager.GetLogFileName();

  // open log file and file descriptor
  // we open it in append + binary mode
  auto log_file = fopen(log_file_name.c_str(), "r");
  if (log_file == NULL) {
    LOG_ERROR("LogFile is NULL");
  }

  // also, get the descriptor
  auto log_file_fd = fileno(log_file);
  if (log_file_fd == -1) {
    LOG_ERROR("log_file_fd is -1");
  }

  fstat(log_file_fd, &log_stats);
  auto log_file_size = log_stats.st_size;

  return log_file_size;
}

}  // namespace logger
}  // namespace benchmark
}  // namespace peloton
