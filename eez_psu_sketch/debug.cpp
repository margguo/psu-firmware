/*
 * EEZ PSU Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "psu.h"
#include "datetime.h"

#if CONF_DEBUG

#define AVG_LOOP_DURATION_N 100

namespace eez {
namespace psu {
namespace debug {

DebugValueVariable g_uDac[2]    = { DebugValueVariable("CH1 U_DAC"),     DebugValueVariable("CH2 U_DAC")     };
DebugValueVariable g_uMon[2]    = { DebugValueVariable("CH1 U_MON"),     DebugValueVariable("CH2 U_MON")     };
DebugValueVariable g_uMonDac[2] = { DebugValueVariable("CH1 U_MON_DAC"), DebugValueVariable("CH2 U_MON_DAC") };
DebugValueVariable g_iDac[2]    = { DebugValueVariable("CH1 I_DAC"),     DebugValueVariable("CH2 I_DAC")     };
DebugValueVariable g_iMon[2]    = { DebugValueVariable("CH1 I_MON"),     DebugValueVariable("CH2 I_MON")     };
DebugValueVariable g_iMonDac[2] = { DebugValueVariable("CH1 I_MON_DAC"), DebugValueVariable("CH2 I_MON_DAC") };

DebugDurationVariable g_mainLoopDuration("MAIN_LOOP_DURATION");
#if CONF_DEBUG_VARIABLES
DebugDurationVariable g_listTickDuration("LIST_TICK_DURATION");
#endif
DebugCounterVariable g_adcCounter("ADC_COUNTER");

DebugVariable *g_variables[] = {
    &g_uDac[0],    &g_uDac[1],
    &g_uMon[0],    &g_uMon[1],
    &g_uMonDac[0], &g_uMonDac[1],
    &g_iDac[0],    &g_iDac[1],
    &g_iMon[0],    &g_iMon[1],
    &g_iMonDac[0], &g_iMonDac[1],

    &g_mainLoopDuration,
#if CONF_DEBUG_VARIABLES
    &g_listTickDuration,
#endif
    &g_adcCounter
};

bool g_debugWatchdog = true;

static uint32_t g_previousTickCount1sec;
static uint32_t g_previousTickCount10sec;

void dumpVariables(char *buffer) {
    buffer[0] = 0;

    for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
        strcat(buffer, g_variables[i]->name());
        strcat(buffer, " = ");
        g_variables[i]->dump(buffer);
        strcat(buffer, "\n");
    }
}

}
}
} // namespace eez::psu::debug

#endif // CONF_DEBUG


#if CONF_DEBUG || CONF_DEBUG_LATEST

namespace eez {
namespace psu {
namespace debug {

static char traceBuffer[256];
static bool dumpTraceBufferOnNextTick = false;

void DumpTraceBuffer() {
    Serial.print("**TRACE");
    
    char datetime_buffer[20] = { 0 };
    if (datetime::getDateTimeAsString(datetime_buffer)) {
        Serial.print(" [");
        Serial.print(datetime_buffer);
        Serial.print("]: ");
    } else {
        Serial.print(": ");
    }

    Serial.println(traceBuffer);

    Serial.flush();
}

void tick(uint32_t tickCount) {
#if CONF_DEBUG
    debug::g_mainLoopDuration.tick(tickCount);

    if (g_previousTickCount1sec != 0) {
        if (tickCount - g_previousTickCount1sec >= 1000000L) {
            for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
                g_variables[i]->tick1secPeriod();
            }
            g_previousTickCount1sec = tickCount;
        }
    } else {
        g_previousTickCount1sec = tickCount;
    }

    if (g_previousTickCount10sec != 0) {
        if (tickCount - g_previousTickCount10sec >= 10 * 1000000L) {
            for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
                g_variables[i]->tick10secPeriod();
            }
            g_previousTickCount10sec = tickCount;
        }
    } else {
        g_previousTickCount10sec = tickCount;
    }
#endif

    if (dumpTraceBufferOnNextTick) {
        DumpTraceBuffer();
        dumpTraceBufferOnNextTick = false;
    }
}

void Trace(const char *format, ...) {
    if (dumpTraceBufferOnNextTick) return;

    va_list args;
    va_start(args, format);
    vsnprintf_P(traceBuffer, sizeof(traceBuffer), format, args);

    if (g_insideInterruptHandler) {
        dumpTraceBufferOnNextTick = true;
    } else {
        DumpTraceBuffer();
    }
}

////////////////////////////////////////////////////////////////////////////////

DebugVariable::DebugVariable(const char *name) : m_name(name) {
}

const char *DebugVariable::name() {
    return m_name;
}

////////////////////////////////////////////////////////////////////////////////

DebugValueVariable::DebugValueVariable(const char *name) : DebugVariable(name) {
}

void DebugValueVariable::tick1secPeriod() {
}

void DebugValueVariable::tick10secPeriod() {
}

void DebugValueVariable::dump(char *buffer) {
    util::strcatInt32(buffer, m_value);
}

////////////////////////////////////////////////////////////////////////////////

DebugDurationForPeriod::DebugDurationForPeriod() 
    : m_min(4294967295)
    , m_max(0)
    , m_total(0)
    , m_count(0)
{
}

void DebugDurationForPeriod::tick(uint32_t duration) {
    if (duration < m_min) {
        m_min = duration;
    }

    if (duration > m_max) {
        m_max = duration;
    }

    m_total += duration;
    ++m_count;
}

void DebugDurationForPeriod::tickPeriod() {
    if (m_count > 0) {
        m_minLast = m_min;
        m_maxLast = m_max;
        m_avgLast = m_total / m_count;
    } else {
        m_minLast = 0;
        m_maxLast = 0;
        m_avgLast = 0;
    }

    m_min = 4294967295;
    m_max = 0;
    m_total = 0;
    m_count = 0;
}


void DebugDurationForPeriod::dump(char *buffer) {
    util::strcatUInt32(buffer, m_minLast);
    strcat(buffer, " ");
    util::strcatUInt32(buffer, m_avgLast);
    strcat(buffer, " ");
    util::strcatUInt32(buffer, m_maxLast);
}

////////////////////////////////////////////////////////////////////////////////

DebugDurationVariable::DebugDurationVariable(const char *name) 
    : DebugVariable(name) 
    , m_minTotal(4294967295)
    , m_maxTotal(0)
{
}

void DebugDurationVariable::start() {
    m_lastTickCount = micros();
}

void DebugDurationVariable::finish() {
    tick(micros());
}

void DebugDurationVariable::tick(uint32_t tickCount) {
    uint32_t duration = tickCount - m_lastTickCount;

    duration1sec.tick(duration);
    duration10sec.tick(duration);

    if (duration < m_minTotal) {
        m_minTotal = duration;
    }

    if (duration > m_maxTotal) {
        m_maxTotal = duration;
    }

    m_lastTickCount = tickCount;
}

void DebugDurationVariable::tick1secPeriod() {
    duration1sec.tickPeriod();
}

void DebugDurationVariable::tick10secPeriod() {
    duration10sec.tickPeriod();
}

void DebugDurationVariable::dump(char *buffer) {
    duration1sec.dump(buffer);

    strcat(buffer, " / ");

    duration10sec.dump(buffer);

    strcat(buffer, " / ");

    util::strcatUInt32(buffer, m_minTotal);
    strcat(buffer, " ");
    util::strcatUInt32(buffer, m_maxTotal);
}

////////////////////////////////////////////////////////////////////////////////

DebugCounterForPeriod::DebugCounterForPeriod() : m_counter(0) {
}

void DebugCounterForPeriod::inc() {
    ++m_counter;
}

void DebugCounterForPeriod::tickPeriod() {
    noInterrupts();
    m_lastCounter = m_counter;
    m_counter = 0;
    interrupts();
}

void DebugCounterForPeriod::dump(char *buffer) {
    util::strcatUInt32(buffer, m_lastCounter);
}

////////////////////////////////////////////////////////////////////////////////

DebugCounterVariable::DebugCounterVariable(const char *name) : DebugVariable(name)
{
}

void DebugCounterVariable::inc() {
    counter1sec.inc();
    counter10sec.inc();
    ++m_totalCounter;
}

void DebugCounterVariable::tick1secPeriod() {
    counter1sec.tickPeriod();
}

void DebugCounterVariable::tick10secPeriod() {
    counter10sec.tickPeriod();
}

void DebugCounterVariable::dump(char *buffer) {
    counter1sec.dump(buffer);

    strcat(buffer, " / ");

    counter10sec.dump(buffer);

    strcat(buffer, " / ");

    util::strcatUInt32(buffer, m_totalCounter);
}

}
}
} // namespace eez::psu::debug

#endif // CONF_DEBUG || CONF_DEBUG_LATEST