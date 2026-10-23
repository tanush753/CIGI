//Copyright SimBlocks LLC 2016-2026
/**
 * @file main.cpp
 * @brief Entry point for the CIGIHost application.
 *
 * Initializes global resources, loads host options, and manages the main event loop for the CIGIHost application.
 * Handles user input for running scripts and resetting the host, and maintains the main update loop with frame rate control.
 * Integrates with the CIGI protocol host, event dispatcher, and logger for simulation interoperability.
 *
 * @see CCIGIHostOptions
 * @see sbio::cigi::host::CHost
 * @see sbio::cigi::host::CScriptRuntime
 * @see sbio::utils::CEventDispatcher
 * @see sbio::utils::CLogger
 */
#include "GlobalHeaders/Globals.h"
#include "HostCigiLib/Host.h"
#include "HostCigiLib/HostSession.h"
#include "HostCigiLib/HostCigiLib.h"
#include "MathLib/MathTypes.h"
#include "Poco/Net/NetException.h"
#include "UtilitiesLib/EventDispatcher.h"
#include "UtilitiesLib/Logger.h"
#include "UtilitiesLib/StopWatch.h"
#include "UtilitiesLib/UtilitiesLib.h"
#include "BatchRunner.h"
#include "CIGIHostOptions.h"
#include "CigiLib/CigiLib.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#include <conio.h>// Windows-specific header for _kbhit() and _getch()
#else
#include "UtilitiesLib/LinuxUtilities.h"
#endif

using namespace std;
using namespace sbio;
using namespace sbio::utils;
using namespace sbio::math;
using namespace sbio::cigi;
using namespace sbio::cigi::host;

SGlobals g_globals;

CHost* g_pHost = nullptr;

const long SLEEP_MS = 17;

/**
 * @brief Runs the --batch request sequence until it finishes.
 * @param pEventDispatcher Dispatcher the host library raises its events on; the batch runner listens there.
 * @return 0 if every request was answered, 1 on timeout or network error, 2 if the IG never connected.
 */
int RunBatch(const shared_ptr<CEventDispatcher>& pEventDispatcher)
{
  CBatchRunner batchRunner(g_pHost);
  pEventDispatcher->RegisterListener<HostCigiEvent>(&batchRunner);

  int nExitCode = 1;
  CStopWatch stopWatch;
  try
  {
    while (!batchRunner.IsDone())
    {
      double fDeltaTime = stopWatch.GetElapsedSeconds();

      stopWatch.Reset();
      stopWatch.Start();

      // replies are raised as events from inside Update, so batchRunner sees them before its own Update runs
      g_pHost->Update(fDeltaTime);
      batchRunner.Update();

      double fDuration = stopWatch.GetElapsedMilliSeconds();

      // maintain frame rate
      if (fDuration < SLEEP_MS)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
      }
    }
    nExitCode = batchRunner.GetExitCode();
  }
  catch (const Poco::Net::NetException& e)
  {
    std::cerr << e.displayText() << std::endl;
  }

  // unregister before batchRunner goes out of scope so the dispatcher never holds a dangling pointer
  pEventDispatcher->UnregisterListener<HostCigiEvent>(&batchRunner);
  return nExitCode;
}

/**
 * @brief Main entry point for the CIGIHost application.
 *
 * Configures application services, loads "CIGI Host/CIGI Host.ini" from the application data directory, and
 * initializes the host. Each iteration updates the host, then handles key '1' to execute the configured script
 * or key '2' to send a reset through the script runtime.
 *
 * With --batch, no script runs: the host connects, sends one request of each terrain query type, prints the
 * replies and exits with the code from RunBatch().
 */
int main(int argc, char* argv[])
{
  bool bBatch = false;
  for (int i = 1; i < argc; i++)
  {
    if (string(argv[i]) == "--batch")
    {
      bBatch = true;
    }
  }

  SGlobals globals;
  globals.applicationsDataPath = GetSdkPath() / "Data" / "Applications";
  globals.librariesDataPath = GetSdkPath() / "Data" / "Libraries";
  globals.thirdPartyPath = sbio::utils::GetEnvVariablePath("SBIO_THIRD_PARTY");
  globals.pEventDispatcher = make_shared<CEventDispatcher>();
  globals.pLogger = make_shared<CLogger>("Host");

  // CCIGIHostOptions reads g_globals to resolve .ini paths, so it must be filled before LoadOptions (same as HostEmulator)
  g_globals.pEventDispatcher = globals.pEventDispatcher;
  g_globals.pLogger = globals.pLogger;
  g_globals.applicationsDataPath = globals.applicationsDataPath;
  g_globals.librariesDataPath = globals.librariesDataPath;

  InitCigiLib();
  InitUtilitiesLib(globals);

  CCIGIHostOptions hostOptions;
  hostOptions.LoadOptions(globals.applicationsDataPath / "CIGI Host" / "CIGI Host.ini");

  g_pHost = new CHost();
  InitHostCigiLib(globals, g_pHost);

  g_pHost->Initialize(hostOptions.hostSetupOptions);

  if (bBatch)
  {
    return RunBatch(globals.pEventDispatcher);
  }

  CStopWatch stopWatch;

  cout << "\nCommands:" << endl;
  cout << "1 - Run Script" << endl;
  cout << "2 - Reset" << endl;
  cout << endl;

  try
  {
    while (true)
    {
      double fDeltaTime = stopWatch.GetElapsedSeconds();

      stopWatch.Reset();
      stopWatch.Start();

      g_pHost->Update(fDeltaTime);

      if (_kbhit())
      {
        char c = static_cast<char>(_getch());
        if (c == '1')
        {
          cout << "\nRunning Script!" << endl;
          g_pHost->GetScriptRuntime()->Execute(hostOptions.hostSetupOptions.sScriptCategory, hostOptions.hostSetupOptions.sScript);
        }
        else if (c == '2')
        {
          cout << "\nReset!" << endl;
          g_pHost->GetScriptRuntime()->SendReset();
        }
      }

      double fDuration = stopWatch.GetElapsedMilliSeconds();

      // maintain frame rate
      if (fDuration < SLEEP_MS)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
      }
    }
  }
  catch (const Poco::Net::NetException& e)
  {
    std::cerr << e.displayText() << std::endl;
  }

  return 0;
}

//The source code in this file is licensed under the MIT License. See the LICENSE text file for full terms.
//Refer all inquiries to sales@simblocks.io
//Copyright SimBlocks LLC 2016-2026
