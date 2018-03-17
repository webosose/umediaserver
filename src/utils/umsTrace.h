#ifndef __UMS_TRACE_H
#define __UMS_TRACE_H

#ifdef LTTNG_PMTRACE

#include <PmTrace.h>

#define UMSTRACE(l)        PMTRACE(const_cast<char*>(l))
#define UMSTRACE_BEFORE(l) PMTRACE_BEFORE(const_cast<char*>(l))
#define UMSTRACE_AFTER(l)  PMTRACE_AFTER(const_cast<char*>(l))

#else

#define UMSTRACE(_)
#define UMSTRACE_BEFORE(_)
#define UMSTRACE_AFTER(_)

#endif

#endif
