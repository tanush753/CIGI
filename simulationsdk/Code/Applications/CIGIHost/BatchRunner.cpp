//The source code in this file is licensed under the MIT License. See the LICENSE text file for full terms.
#include "BatchRunner.h"
#include "CigiLib/CigiTypesHostToIG.h"
#include "HostCigiLib/HostSession.h"
#include <iomanip>
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace sbio;
using namespace sbio::math;
using namespace sbio::cigi;
using namespace sbio::cigi::host;

namespace
{
  const double CONNECT_TIMEOUT_SECONDS = 10.0;
  const double REPLY_TIMEOUT_SECONDS = 5.0;
  const int DATABASE_NUMBER = 2;

  // Library messages end with a newline, which would leave blank lines between our one-line events.
  string TrimNewlines(string sText)
  {
    while (!sText.empty() && (sText.back() == '\n' || sText.back() == '\r'))
    {
      sText.pop_back();
    }
    return sText;
  }
}

CBatchRunner::CBatchRunner(CHost* pHost)
  : m_pHost(pHost)
{
  m_startTime = steady_clock::now();
  m_stateStartTime = m_startTime;
  cout << fixed;
}

void CBatchRunner::Update()
{
  CHostSession* pHostSession = m_pHost->GetHostSession();

  switch (m_eState)
  {
  case EBatchState::CONNECTING:
    // SetIGControl raises an error event on every call while disconnected, so wait for the connection first
    if (pHostSession != nullptr && pHostSession->IsConnected())
    {
      if (pHostSession->SetIGControl(CigiDatabaseNumber(DATABASE_NUMBER), false, EIGMode::OPERATE, true))
      {
        cout << "INFO  connected, requesting database " << DATABASE_NUMBER << endl;
        m_eState = EBatchState::WAITING_FOR_DATABASE;
        m_stateStartTime = steady_clock::now();
      }
    }
    else if (GetSecondsInState() > CONNECT_TIMEOUT_SECONDS)
    {
      cout << "ERROR not connected to the IG after " << setprecision(0) << CONNECT_TIMEOUT_SECONDS << " s. Start the IG and press 1 (Play) first." << endl;
      Finish(2);
    }
    break;

  case EBatchState::WAITING_FOR_DATABASE:
    if (m_bDatabaseLoaded)
    {
      SendRequests();
      m_eState = EBatchState::WAITING_FOR_REPLIES;
      m_stateStartTime = steady_clock::now();
    }
    else if (GetSecondsInState() > CONNECT_TIMEOUT_SECONDS)
    {
      cout << "ERROR database " << DATABASE_NUMBER << " did not finish loading" << endl;
      Finish(1);
    }
    break;

  case EBatchState::WAITING_FOR_REPLIES:
    if (CountAnswered() == static_cast<int>(m_HatHotRequests.size() + m_LineOfSightRequests.size()))
    {
      Finish(0);
    }
    else if (GetSecondsInState() > REPLY_TIMEOUT_SECONDS)
    {
      Finish(1);
    }
    break;

  case EBatchState::DONE:
    break;
  }
}

bool CBatchRunner::IsDone() const
{
  return m_eState == EBatchState::DONE;
}

int CBatchRunner::GetExitCode() const
{
  return m_nExitCode;
}

void CBatchRunner::SendRequests()
{
  CHostSession* pHostSession = m_pHost->GetHostSession();

  SHATHOTGlobalRequest hatRequest;
  hatRequest.requestID = HATHOTID(1);
  hatRequest.updatePeriod = UpdatePeriod(0);
  hatRequest.eRequestType = ERequestType::HAT;
  hatRequest.geodeticCoordinates.latitude = Latitude(0.0);
  hatRequest.geodeticCoordinates.longitude = Longitude(0.0);
  hatRequest.geodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(100.0);
  SendHatHotRequest(hatRequest, "HAT");

  SHATHOTGlobalRequest hotRequest;
  hotRequest.requestID = HATHOTID(2);
  hotRequest.updatePeriod = UpdatePeriod(0);
  hotRequest.eRequestType = ERequestType::HOT;
  hotRequest.geodeticCoordinates.latitude = Latitude(0.001);
  hotRequest.geodeticCoordinates.longitude = Longitude(0.0);
  hotRequest.geodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(0.0);
  SendHatHotRequest(hotRequest, "HOT");

  SHATHOTGlobalRequest extendedRequest;
  extendedRequest.requestID = HATHOTID(3);
  extendedRequest.updatePeriod = UpdatePeriod(0);
  extendedRequest.eRequestType = ERequestType::EXTENDED;
  extendedRequest.geodeticCoordinates.latitude = Latitude(0.0);
  extendedRequest.geodeticCoordinates.longitude = Longitude(0.001);
  extendedRequest.geodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(50.0);
  SendHatHotRequest(extendedRequest, "EXT");

  SLineOfSightSegmentRequestGeodeticToGeodeticBasic segmentRequest;
  segmentRequest.requestID = LineOfSightRequestID(1);
  segmentRequest.updatePeriod = UpdatePeriod(0);
  segmentRequest.nAlphaThreshold = 1;
  segmentRequest.nMaterialMask = 255;
  segmentRequest.sourceGeodeticCoordinates.latitude = Latitude(0.0);
  segmentRequest.sourceGeodeticCoordinates.longitude = Longitude(0.0);
  segmentRequest.sourceGeodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(100.0);
  segmentRequest.destinationGeodeticCoordinates.latitude = Latitude(0.001);
  segmentRequest.destinationGeodeticCoordinates.longitude = Longitude(0.0);
  segmentRequest.destinationGeodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(100.0);
  cout << "SENT  LOS   id=1  segment " << setprecision(6)
    << "(" << segmentRequest.sourceGeodeticCoordinates.latitude.Value() << "," << segmentRequest.sourceGeodeticCoordinates.longitude.Value()
    << "," << setprecision(1) << segmentRequest.sourceGeodeticCoordinates.altitude.Value() << ") -> " << setprecision(6)
    << "(" << segmentRequest.destinationGeodeticCoordinates.latitude.Value() << "," << segmentRequest.destinationGeodeticCoordinates.longitude.Value()
    << "," << setprecision(1) << segmentRequest.destinationGeodeticCoordinates.altitude.Value() << ")" << endl;
  m_LineOfSightRequests[1].sentTime = steady_clock::now();
  pHostSession->SendLineOfSightSegmentRequestGeodeticToGeodeticBasic(segmentRequest);

  SLineOfSightVectorRequestGeodeticBasic vectorRequest;
  vectorRequest.requestID = LineOfSightRequestID(2);
  vectorRequest.updatePeriod = UpdatePeriod(0);
  vectorRequest.nAlphaThreshold = 1;
  vectorRequest.nMaterialMask = 255;
  vectorRequest.sourceGeodeticCoordinates.latitude = Latitude(0.0);
  vectorRequest.sourceGeodeticCoordinates.longitude = Longitude(0.0);
  vectorRequest.sourceGeodeticCoordinates.altitude = HeightRelativeToWGS84Ellipsoid(100.0);
  vectorRequest.azimuth = Degrees180(0.0);
  vectorRequest.elevation = Degrees90(-10.0);
  vectorRequest.fMinimumRange = 0.0f;
  vectorRequest.fMaximumRange = 1000.0f;
  cout << "SENT  LOS   id=2  vector " << setprecision(6)
    << "(" << vectorRequest.sourceGeodeticCoordinates.latitude.Value() << "," << vectorRequest.sourceGeodeticCoordinates.longitude.Value()
    << "," << setprecision(1) << vectorRequest.sourceGeodeticCoordinates.altitude.Value() << ")"
    << " az=" << vectorRequest.azimuth.Value() << " el=" << vectorRequest.elevation.Value()
    << " range=" << vectorRequest.fMinimumRange << ".." << vectorRequest.fMaximumRange << endl;
  m_LineOfSightRequests[2].sentTime = steady_clock::now();
  pHostSession->SendLineOfSightVectorRequestGeodeticBasic(vectorRequest);
}

void CBatchRunner::SendHatHotRequest(const SHATHOTGlobalRequest& request, const string& sName)
{
  cout << "SENT  " << left << setw(6) << sName << right << "id=" << request.requestID.Value() << setprecision(6)
    << "  lat=" << request.geodeticCoordinates.latitude.Value()
    << " lon=" << request.geodeticCoordinates.longitude.Value()
    << " alt=" << setprecision(1) << request.geodeticCoordinates.altitude.Value() << endl;
  m_HatHotRequests[request.requestID.Value()].sentTime = steady_clock::now();
  m_pHost->GetHostSession()->SendHatHotRequest(request);
}

void CBatchRunner::Finish(int nExitCode)
{
  m_eState = EBatchState::DONE;
  m_nExitCode = nExitCode;

  const size_t nSent = m_HatHotRequests.size() + m_LineOfSightRequests.size();
  const double fSeconds = duration<double>(steady_clock::now() - m_startTime).count();
  cout << "DONE  sent=" << nSent << " answered=" << CountAnswered()
    << " time=" << setprecision(1) << fSeconds << " s exit=" << nExitCode << endl;
}

int CBatchRunner::CountAnswered() const
{
  int nAnswered = 0;
  for (const auto& it : m_HatHotRequests)
  {
    if (it.second.nRepliesReceived >= it.second.nRepliesExpected)
    {
      nAnswered++;
    }
  }
  for (const auto& it : m_LineOfSightRequests)
  {
    if (it.second.nRepliesReceived >= it.second.nRepliesExpected)
    {
      nAnswered++;
    }
  }
  return nAnswered;
}

double CBatchRunner::GetSecondsInState() const
{
  return duration<double>(steady_clock::now() - m_stateStartTime).count();
}

string CBatchRunner::RecordReply(map<uint16_t, SPendingRequest>& requests, uint16_t nID, int nResponseCount)
{
  auto it = requests.find(nID);
  if (it == requests.end())
  {
    return "  (unmatched)";
  }

  SPendingRequest& request = it->second;
  request.nRepliesReceived++;

  string sResult;
  // The IG sends one LOS reply per intersection, and every reply carries the total, so the latest one tells us how many to wait for
  if (nResponseCount > 0)
  {
    request.nRepliesExpected = nResponseCount;
    sResult = " count=" + to_string(request.nRepliesReceived) + "/" + to_string(nResponseCount);
  }

  const long long nMilliseconds = duration_cast<milliseconds>(steady_clock::now() - request.sentTime).count();
  return sResult + "  (" + to_string(nMilliseconds) + " ms)";
}

void CBatchRunner::PrintExtendedLineOfSight(const SBaseLineOfSightExtendedResponse& response, const string& sEntity)
{
  cout << "RECV  LOSX  id=" << response.lineOfSightRequestID.Value()
    << "  valid=" << response.bValid << " rangeValid=" << response.bRangeValid << " visible=" << response.bVisible
    << " entity=" << sEntity << " range=" << setprecision(3) << response.dRange
    << RecordReply(m_LineOfSightRequests, response.lineOfSightRequestID.Value(), response.responseCount) << endl;
}

void CBatchRunner::OnDatabaseLoaded(const HostCigiDatabaseLoadedEventArgs& args)
{
  cout << "INFO  database " << static_cast<int>(args.eDatabaseID.Value()) << " loaded" << endl;
  if (args.eDatabaseID.Value() == DATABASE_NUMBER)
  {
    m_bDatabaseLoaded = true;
  }
}

void CBatchRunner::OnHostCigiErrorEvent(const HostCigiErrorEventArgs& args)
{
  cout << "ERROR " << TrimNewlines(args.sError) << endl;
}

void CBatchRunner::OnHostCigiMessageEvent(const HostCigiMessageEventArgs& args)
{
  cout << "INFO  " << TrimNewlines(args.sMessage) << endl;
}

void CBatchRunner::OnHostCigiHatResponseEvent(const HostCigiHatResponseEventArgs& args)
{
  const SHeightAboveTerrainResponse& response = args.hatResponse;
  cout << "RECV  HAT   id=" << response.HATHOTID.Value()
    << "  valid=" << response.bValid << " height=" << setprecision(3) << response.heightAboveTerrain
    << RecordReply(m_HatHotRequests, response.HATHOTID.Value(), 0) << endl;
}

void CBatchRunner::OnHostCigiHotResponseEvent(const HostCigiHotResponseEventArgs& args)
{
  const SHeightOfTerrainResponse& response = args.hotResponse;
  cout << "RECV  HOT   id=" << response.HATHOTID.Value()
    << "  valid=" << response.bValid << " height=" << setprecision(3) << response.heightOfTerrain.Value()
    << RecordReply(m_HatHotRequests, response.HATHOTID.Value(), 0) << endl;
}

void CBatchRunner::OnHostCigiHatHotExtendedResponseEvent(const HostCigiHatHotExtendedResponseEventArgs& args)
{
  const SHATHOTExtendedResponse& response = args.hatHotExtendedResponse;
  cout << "RECV  EXT   id=" << response.HATHOTID.Value()
    << "  valid=" << response.bValid << " hat=" << setprecision(3) << response.heightAboveTerrain
    << " hot=" << response.heightOfTerrain.Value() << " material=" << response.materialCode.Value()
    << " normal=" << setprecision(1) << response.normalVectorAzimuth.Value() << "/" << response.normalVectorElevation.Value()
    << RecordReply(m_HatHotRequests, response.HATHOTID.Value(), 0) << endl;
}

void CBatchRunner::OnHostCigiLineOfSightResponseEvent(const HostCigiLineOfSightResponseEventArgs& args)
{
  const SLineOfSightResponse& response = args.lineOfSightResponse;
  // The library does not pass Visible on for non-entity replies, so print "-" rather than guess a value
  cout << "RECV  LOS   id=" << response.lineOfSightRequestID.Value()
    << "  valid=" << response.bValid << " visible=- entity=- range=" << setprecision(3) << response.dRange
    << RecordReply(m_LineOfSightRequests, response.lineOfSightRequestID.Value(), response.responseCount) << endl;
}

void CBatchRunner::OnHostCigiLineOfSightEntityResponseEvent(const HostCigiLineOfSightEntityResponseEventArgs& args)
{
  const SLineOfSightEntityResponse& response = args.lineOfSightEntityResponse;
  cout << "RECV  LOS   id=" << response.lineOfSightRequestID.Value()
    << "  valid=" << response.bValid << " visible=" << response.bVisible << " entity=" << response.entityID.Value()
    << " range=" << setprecision(3) << response.dRange
    << RecordReply(m_LineOfSightRequests, response.lineOfSightRequestID.Value(), response.responseCount) << endl;
}

void CBatchRunner::OnHostCigiLineOfSightExtendedGeodeticCoordinatesResponseEvent(const HostCigiLineOfSightExtendedGeodeticCoordinatesResponseEventArgs& args)
{
  PrintExtendedLineOfSight(args.lineOfSightExtendedGeodeticCoordinatesResponse, "-");
}

void CBatchRunner::OnHostCigiLineOfSightExtendedEntityGeodeticCoordinatesResponseEvent(const HostCigiLineOfSightExtendedEntityGeodeticCoordinatesResponseEventArgs& args)
{
  const SLineOfSightExtendedEntityGeodeticCoordinatesResponse& response = args.lineOfSightExtendedEntityGeodeticCoordinatesResponse;
  PrintExtendedLineOfSight(response, to_string(response.entityID.Value()));
}

void CBatchRunner::OnHostCigiLineOfSightExtendedEntityCoordinatesResponseEvent(const HostCigiLineOfSightExtendedEntityCoordinatesResponseEventArgs& args)
{
  const SLineOfSightExtendedEntityCoordinatesResponse& response = args.lineOfSightExtendedEntityCoordinatesResponse;
  PrintExtendedLineOfSight(response, to_string(response.entityID.Value()));
}
