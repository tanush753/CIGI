//The source code in this file is licensed under the MIT License. See the LICENSE text file for full terms.
/**
 * @file BatchRunner.h
 * @brief Declares CBatchRunner, which drives the CIGIHost --batch mode.
 *
 * Batch mode connects to the IG, loads a database, sends one HAT, HOT, extended HAT/HOT, LOS segment and LOS vector
 * request, prints every request and reply, and reports an exit code.
 */
#pragma once
#ifndef SIMBLOCKS_CIGI_HOST_BATCH_RUNNER_H
#define SIMBLOCKS_CIGI_HOST_BATCH_RUNNER_H

#include "CigiLib/CigiTypesHostToIG.h"
#include "CigiLib/CigiTypesIGToHost.h"
#include "HostCigiLib/Host.h"
#include "HostCigiLib/HostCigiEvent.h"
#include <chrono>
#include <cstdint>
#include <map>
#include <string>

/**
 * @brief Runs the batch request sequence and prints the results.
 *
 * Call Update() once per frame after CHost::Update(). Replies arrive through the IHostCigiEventListener callbacks,
 * which the host library raises from inside CHost::Update(), so everything runs on the main thread.
 */
class CBatchRunner : public sbio::cigi::host::IHostCigiEventListener
{
public:
  /**
   * @brief Creates a runner for the given host.
   * @param pHost Initialized host used to reach the session. Not owned.
   */
  explicit CBatchRunner(sbio::cigi::host::CHost* pHost);

  /**
   * @brief Advances the state machine: connect, wait for the database, send the requests, then wait for replies.
   */
  void Update();

  /** @brief Returns true once the run has finished and the exit code is set. */
  bool IsDone() const;

  /** @brief Returns 0 if every request was answered, 1 on timeout, 2 if the IG never connected. */
  int GetExitCode() const;

  void OnDatabaseLoaded(const sbio::cigi::host::HostCigiDatabaseLoadedEventArgs& args) override;
  void OnHostCigiErrorEvent(const sbio::cigi::host::HostCigiErrorEventArgs& args) override;
  void OnHostCigiMessageEvent(const sbio::cigi::host::HostCigiMessageEventArgs& args) override;
  void OnHostCigiHatResponseEvent(const sbio::cigi::host::HostCigiHatResponseEventArgs& args) override;
  void OnHostCigiHotResponseEvent(const sbio::cigi::host::HostCigiHotResponseEventArgs& args) override;
  void OnHostCigiHatHotExtendedResponseEvent(const sbio::cigi::host::HostCigiHatHotExtendedResponseEventArgs& args) override;
  void OnHostCigiLineOfSightResponseEvent(const sbio::cigi::host::HostCigiLineOfSightResponseEventArgs& args) override;
  void OnHostCigiLineOfSightEntityResponseEvent(const sbio::cigi::host::HostCigiLineOfSightEntityResponseEventArgs& args) override;
  void OnHostCigiLineOfSightExtendedGeodeticCoordinatesResponseEvent(const sbio::cigi::host::HostCigiLineOfSightExtendedGeodeticCoordinatesResponseEventArgs& args) override;
  void OnHostCigiLineOfSightExtendedEntityGeodeticCoordinatesResponseEvent(const sbio::cigi::host::HostCigiLineOfSightExtendedEntityGeodeticCoordinatesResponseEventArgs& args) override;
  void OnHostCigiLineOfSightExtendedEntityCoordinatesResponseEvent(const sbio::cigi::host::HostCigiLineOfSightExtendedEntityCoordinatesResponseEventArgs& args) override;

private:
  enum class EBatchState
  {
    CONNECTING,
    WAITING_FOR_DATABASE,
    WAITING_FOR_REPLIES,
    DONE,
  };

  /** @brief A request that has been sent, kept until all of its replies arrive. */
  struct SPendingRequest
  {
    std::chrono::steady_clock::time_point sentTime;
    int nRepliesReceived = 0;
    int nRepliesExpected = 1;///< 1 for HAT/HOT. For LOS it is taken from Response Count in the first reply.
  };

  void SendRequests();
  void SendHatHotRequest(const sbio::cigi::SHATHOTGlobalRequest& request, const std::string& sName);
  void Finish(int nExitCode);
  int CountAnswered() const;
  double GetSecondsInState() const;

  /**
   * @brief Marks one reply as received and returns the text that ends its RECV line.
   * @param nResponseCount Response Count from a LOS reply, or 0 for HAT/HOT replies.
   * @return " count=received/total" for LOS, then the latency in ms, or "(unmatched)" if the ID was never sent.
   */
  std::string RecordReply(std::map<uint16_t, SPendingRequest>& requests, uint16_t nID, int nResponseCount);
  void PrintExtendedLineOfSight(const sbio::cigi::SBaseLineOfSightExtendedResponse& response, const std::string& sEntity);

  sbio::cigi::host::CHost* m_pHost = nullptr;
  EBatchState m_eState = EBatchState::CONNECTING;
  std::chrono::steady_clock::time_point m_startTime;
  std::chrono::steady_clock::time_point m_stateStartTime;
  bool m_bDatabaseLoaded = false;
  int m_nExitCode = -1;
  // HAT/HOT IDs and LOS IDs are separate ID spaces in CIGI, so they need separate maps.
  std::map<uint16_t, SPendingRequest> m_HatHotRequests;
  std::map<uint16_t, SPendingRequest> m_LineOfSightRequests;
};

#endif
