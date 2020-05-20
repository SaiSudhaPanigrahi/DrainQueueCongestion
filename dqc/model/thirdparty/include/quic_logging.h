#pragma once
#include "logging.h"
#define QUIC_DLOG(severity) DLOG(severity)
#define QUIC_LOG_IF(severity, condition) DLOG_IF(severity, condition)
#define QUIC_BUG  LOG(FATAL)
#define QUIC_BUG_IF(condition) LOG_IF(FATAL, condition)
