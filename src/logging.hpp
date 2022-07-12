#pragma once

#include <easylogging++.h>
#define NLOG(LEVEL) CLOG(LEVEL, "consoleLogger", "default")

static std::string g_executable;
static bool g_interrupted = false;

// *****************************************************************************
void crash_handler( int sig ) {
  g_interrupted = true;
  // associate each signal with a signal name string.
  const char* name = nullptr;
  switch( sig ) {
    case SIGABRT: name = "SIGABRT";  break;
    case SIGFPE:  name = "SIGFPE";   break;
    case SIGILL:  name = "SIGILL";   break;
    case SIGINT:  name = "SIGINT";   break;
    case SIGSEGV: name = "SIGSEGV";  break;
    case SIGTERM: name = "SIGTERM";  break;
  }

  if (sig == SIGINT || sig == SIGTERM) {
    if (name) {
      NLOG(DEBUG) << "Terminating " << g_executable << ", signal: "
                  << sig << " (" << name << ')';
    } else {
      NLOG(DEBUG) << "Terminating " << g_executable << ", signal: "
                  << sig;
    }
  } else {
    NLOG(ERROR) << "Crashed " << g_executable;
    el::Helpers::logCrashReason( sig, true );
  }
  // FOLLOWING LINE IS ABSOLUTELY NEEDED TO ABORT APPLICATION
  el::Helpers::crashAbort( sig );
}

// *****************************************************************************
static void setup_logging( const std::string& executable,
                           void (*crash_handler)(int),
                           const std::string& logfile = {},
                           bool file_logging = false,
                           bool console_logging = false )
{
  g_executable = executable;
  el::Configurations fileLogConf;
  fileLogConf.parseFromText(
    "* GLOBAL:\n ENABLED = " + std::to_string(file_logging) + '\n' +
                 "FORMAT = \"%datetime %host " + g_executable +
                           "[%thread]: %level: %msg\"\n"
                 "FILENAME = " + logfile + '\n' +
                 "TO_FILE = true\n"
                 "TO_STANDARD_OUTPUT = false\n"
                 "MAX_LOG_FILE_SIZE = 2097152\n"
    "* DEBUG:\n ENABLED = true\n" );
  el::Configurations consoleLogConf;
  consoleLogConf.parseFromText(
    "* GLOBAL:\n ENABLED = " + std::to_string(console_logging) + '\n' +
                "FORMAT = \"%msg\"\n"
                "TO_FILE = false\n"
                "TO_STANDARD_OUTPUT = true\n"
    "* DEBUG:\n ENABLED = false\n" );
  el::Loggers::addFlag( el::LoggingFlag::MultiLoggerSupport );
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Helpers::setThreadName( "main" );
  el::Helpers::setCrashHandler( crash_handler );
  el::Logger* consoleLogger = el::Loggers::getLogger( "consoleLogger" );
  el::Loggers::reconfigureLogger( "default", consoleLogConf );
  el::Loggers::reconfigureLogger( consoleLogger, fileLogConf );
}
